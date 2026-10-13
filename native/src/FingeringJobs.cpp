#include <algorithm>
#include <cctype>
#include <fstream>
#include <sightreading/FingeringData.hpp>
#include <sightreading/FingeringJobs.hpp>
namespace sightreading {
using json = nlohmann::json;
FingeringJobs::FingeringJobs(Storage &s, std::filesystem::path p, Clock c)
    : storage(s), root(std::filesystem::absolute(p)), clock(std::move(c)) {}
FingeringJobs::~FingeringJobs() {
  std::lock_guard serial(lifecycle);
  {
    std::lock_guard lock(mutex);
    if (state["state"] == "running")
      state["state"] = "cancelled";
    if (process)
      process->terminate();
  }
  if (worker.joinable())
    worker.join();
}
json FingeringJobs::snapshot() const {
  std::lock_guard lock(mutex);
  return state;
}
json FingeringJobs::start(const std::string &songId) {
  std::lock_guard serial(lifecycle);
  {
    std::lock_guard lock(mutex);
    if (state["state"] == "running")
      throw FingeringError(409, "fingering_busy",
                           "A fingering job is already running");
  }
  if (worker.joinable())
    worker.join();
  json song;
  try {
    song = storage.song(songId);
  } catch (const std::invalid_argument &e) {
    throw FingeringError(404, "song_not_found", e.what());
  }
  auto runtime = root / ".tools/fingering";
  auto python = runtime / "python/python.exe";
  for (auto p :
       {python, runtime / "models/left_ArGNN.pth",
        runtime / "models/right_ArGNN.pth", root / "fingering/worker.py"})
    if (!std::filesystem::is_regular_file(p))
      throw FingeringError(503, "fingering_setup_missing",
                           "Run scripts/setup-fingering.ps1 to install the "
                           "local fingering runtime and weights");
  static std::atomic<uint64_t> counter{};
  auto jobId =
      "fingering-" +
      std::to_string(
          std::chrono::steady_clock::now().time_since_epoch().count()) +
      "-" + std::to_string(++counter);
  auto request = fingeringRequest(song, jobId);
  std::shared_ptr<FingeringProcess> child;
  try {
    child = std::make_shared<FingeringProcess>(
        python, std::vector<std::wstring>{
                    L"-I", (root / "fingering/worker.py").wstring(),
                    L"--runtime-dir", runtime.wstring()});
  } catch (const std::exception &e) {
    throw FingeringError(503, "fingering_setup_missing", e.what());
  }
  json initial = {{"state", "running"}, {"jobId", jobId},
                  {"songId", songId},   {"stage", "starting"},
                  {"completed", 0},     {"total", request["notes"].size()}};
  {
    std::lock_guard lock(mutex);
    state = initial;
    process = child;
  }
  try {
    worker = std::thread([this, request, child] { run(request, child); });
  } catch (...) {
    child->terminate();
    std::lock_guard lock(mutex);
    state["state"] = "failed";
    process.reset();
    throw;
  }
  return initial;
}
void FingeringJobs::cancel(const std::string &id) {
  std::lock_guard lock(mutex);
  if (state["jobId"] != id || state["state"] != "running")
    throw FingeringError(409, "fingering_stale",
                         "This job is no longer running");
  state["state"] = "cancelled";
  state["stage"] = "cancelled";
  if (process)
    process->terminate();
}
void FingeringJobs::run(json request, std::shared_ptr<FingeringProcess> child) {
  const auto started = clock();
  auto last = started;
  try {
    child->writeFrame(request);
    for (;;) {
      {
        std::lock_guard lock(mutex);
        if (state["state"] != "running")
          break;
      }
      auto now = clock();
      if (now - started >= std::chrono::minutes(15) ||
          now - last >= std::chrono::seconds(30))
        throw std::runtime_error("Fingering worker exceeded its time limit");
      auto frame = child->readFrame(std::chrono::milliseconds(100));
      if (!frame)
        continue;
      if (!frame->is_object() || !frame->contains("version") ||
          !frame->at("version").is_number_integer() ||
          frame->at("version") != 1 ||
          frame->value("jobId", std::string{}) !=
              request["jobId"].get<std::string>())
        throw std::runtime_error(
            "Invalid fingering worker protocol or job identity");
      last = clock();
      auto type = frame->value("type", std::string{});
      if (type == "error") {
        auto code = frame->value("code", std::string{}),
             message = frame->value("message", std::string{});
        if (code.empty() || code.size() > 64 || message.empty() ||
            !std::all_of(code.begin(), code.end(), [](unsigned char c) {
              return std::isalnum(c) || c == '_';
            }))
          throw std::runtime_error("Malformed fingering worker error");
        throw FingeringError(400, code, message.substr(0, 1024));
      }
      std::lock_guard lock(mutex);
      if (state["state"] != "running")
        break;
      if (type == "progress") {
        if (!frame->contains("completed") || !frame->contains("total") ||
            !(*frame)["completed"].is_number_integer() ||
            !(*frame)["total"].is_number_integer())
          throw std::runtime_error("Invalid worker progress");
        auto done = (*frame)["completed"].get<int64_t>(),
             total = (*frame)["total"].get<int64_t>();
        auto stage = frame->value("stage", std::string{});
        if (done < 0 || total < 0 || done > total || stage.size() > 128)
          throw std::runtime_error("Invalid worker progress bounds");
        state["stage"] = stage;
        state["completed"] = done;
        state["total"] = total;
      } else if (type == "result") {
        std::ifstream pinsFile(root / "fingering/dependencies.json");
        auto pins = json::parse(pinsFile);
        auto metadata = frame->at("model");
        if (metadata.at("name") != "ArGNN" ||
            metadata.at("revision") != pins.at("upstream").at("revision") ||
            metadata.at("checkpointHashes").at("left") !=
                pins.at("checkpoints").at("left").at("sha256") ||
            metadata.at("checkpointHashes").at("right") !=
                pins.at("checkpoints").at("right").at("sha256"))
          throw std::runtime_error(
              "Worker model metadata does not match pinned checkpoints");
        // The same mutex serializes cancellation and the atomic storage
        // replacement.
        auto summary = storage.commitFingering(request["songId"],
                                               request["fingerprint"], *frame);
        state["state"] = "completed";
        state["stage"] = "completed";
        state["summary"] = summary.at("fingeringRun");
        break;
      } else
        throw std::runtime_error("Unknown fingering worker message");
    }
  } catch (const std::exception &e) {
    std::lock_guard lock(mutex);
    if (state["state"] == "running") {
      state["state"] = "failed";
      state["stage"] = "failed";
      auto typed = dynamic_cast<const FingeringError *>(&e);
      state["error"] = {{"code", typed ? typed->code : "fingering_failed"},
                        {"message", std::string(e.what()).substr(0, 1024)}};
    }
  }
  child->terminate();
  std::lock_guard lock(mutex);
  process.reset();
}
} // namespace sightreading
