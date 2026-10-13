#include <catch2/catch_test_macros.hpp>
#include <future>
#include <sightreading/FingeringJobs.hpp>
#include <thread>
using namespace sightreading;
using json = nlohmann::json;
static auto root = std::filesystem::path(SR_ROOT);
TEST_CASE("Owned Python pipes frame, bound diagnostics, reject corrupt output",
          "[fingering]") {
  auto python = root / ".tools/fingering/python/python.exe";
  for (auto mode : {L"echo", L"stderr"}) {
    FingeringProcess p(
        python,
        {L"-I", (root / "tests/fixtures/fingering-worker.py").wstring(), mode});
    json value = {{"quoted", "space \" slash\\"}, {"notes", json::array()}};
    p.writeFrame(value);
    REQUIRE(p.readFrame(std::chrono::seconds(5)) == value);
    p.terminate();
    REQUIRE(p.diagnostics().size() <= 4096);
  }
  for (auto mode : {L"truncate", L"oversize", L"crash"}) {
    FingeringProcess p(
        python,
        {L"-I", (root / "tests/fixtures/fingering-worker.py").wstring(), mode});
    REQUIRE_THROWS(p.readFrame(std::chrono::seconds(5)));
  }
}
TEST_CASE("Owned process timeout and termination unblock pipe reads",
          "[fingering]") {
  auto began = std::chrono::steady_clock::now();
  {
    FingeringProcess p(root / ".tools/fingering/python/python.exe",
                       {L"-I",
                        (root / "tests/fixtures/fingering-worker.py").wstring(),
                        L"hang"});
    REQUIRE_FALSE(p.readFrame(std::chrono::milliseconds(20)));
    p.terminate();
    REQUIRE_THROWS(p.readFrame(std::chrono::seconds(1)));
  }
  REQUIRE(std::chrono::steady_clock::now() - began < std::chrono::seconds(5));
}
TEST_CASE("Job construction needs no runtime and missing setup stays lazy",
          "[fingering]") {
  auto folder =
      std::filesystem::temp_directory_path() /
      std::filesystem::path(
          "sr-fingering-" +
          std::to_string(
              std::chrono::steady_clock::now().time_since_epoch().count()));
  {
    Storage storage(folder);
    json song = {{"id", "setup-song"},
                 {"title", "Setup"},
                 {"ppq", 480},
                 {"tracks", json::array()},
                 {"notes", json::array()}};
    storage.putSong(song, "");
    FingeringJobs jobs(storage, folder);
    try {
      jobs.start("setup-song");
      FAIL("Missing runtime must reject generation");
    } catch (const FingeringError &e) {
      REQUIRE(e.status == 503);
    }
    REQUIRE(jobs.snapshot()["state"] == "idle");
    REQUIRE_THROWS_AS(jobs.start("missing"), FingeringError);
  }
  std::filesystem::remove_all(folder);
}
static json jobSong() {
  return {{"id", "job-song"},
          {"title", "Job"},
          {"ppq", 480},
          {"tracks", json::array({{{"id", 0},
                                   {"title", "RH"},
                                   {"staff", "treble"},
                                   {"percussion", false}}})},
          {"notes", json::array({{{"id", 1},
                                  {"trackId", 0},
                                  {"midiNote", 60},
                                  {"startTick", 0},
                                  {"endTick", 480}}})}};
}
TEST_CASE("Injected monotonic deadlines fail without saving", "[fingering]") {
  for (auto seconds : {31, 901}) {
    auto folder =
        std::filesystem::temp_directory_path() /
        std::filesystem::path(
            "sr-deadline-" +
            std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
    {
      Storage storage(folder);
      storage.putSong(jobSong(), "");
      std::atomic<int> ticks{};
      FingeringJobs jobs(storage, root, [&] {
        return std::chrono::steady_clock::time_point{} +
               std::chrono::seconds(seconds * ticks++);
      });
      jobs.start("job-song");
      for (int i = 0; i < 100 && jobs.snapshot()["state"] == "running"; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
      REQUIRE(jobs.snapshot()["state"] == "failed");
      REQUIRE(storage.song("job-song")["fingeringRevision"] == 0);
    }
    std::filesystem::remove_all(folder);
  }
}
TEST_CASE("Cancellation serializes final replacement and shutdown owns worker",
          "[fingering]") {
  auto folder =
      std::filesystem::temp_directory_path() /
      std::filesystem::path(
          "sr-cancel-" +
          std::to_string(
              std::chrono::steady_clock::now().time_since_epoch().count()));
  {
    Storage storage(folder);
    storage.putSong(jobSong(), "");
    for (int i = 0; i < 4; ++i) {
      FingeringJobs jobs(storage, root);
      auto s = jobs.start("job-song");
      REQUIRE_THROWS_AS(jobs.start("job-song"), FingeringError);
      jobs.cancel(s["jobId"].get<std::string>());
      REQUIRE(jobs.snapshot()["state"] == "cancelled");
    }
    REQUIRE(storage.song("job-song")["fingeringRevision"] == 0);
    {
      FingeringJobs jobs(storage, root);
      jobs.start("job-song");
    }
    REQUIRE(storage.song("job-song")["fingeringRevision"] == 0);
  }
  std::filesystem::remove_all(folder);
}
TEST_CASE("Terminating non-reading worker unblocks a large write",
          "[fingering]") {
  FingeringProcess process(
      root / ".tools/fingering/python/python.exe",
      {L"-I", (root / "tests/fixtures/fingering-worker.py").wstring(),
       L"hang"});
  std::atomic<bool> rejected{};
  std::thread writer([&] {
    try {
      process.writeFrame({{"large", std::string(1024 * 1024, 'x')}});
    } catch (...) {
      rejected = true;
    }
  });
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  process.terminate();
  writer.join();
  REQUIRE(rejected);
}
TEST_CASE("Expired final-frame acceptance preserves previous saved fingerings",
          "[fingering][fingering-deadline]") {
  for (auto boundary :
       {std::pair{false, 31}, std::pair{false, 901}, std::pair{true, 901}}) {
    const auto [beforeCommit, expiry] = boundary;
    auto folder =
        std::filesystem::temp_directory_path() /
        std::filesystem::path(
            "sr-final-deadline-" +
            std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
    {
      Storage storage(folder);
      storage.putSong(jobSong(), "");
      storage.editFingering("job-song", {1}, 3);
      auto saved = storage.song("job-song");
      std::atomic<bool> finalRead{};
      std::atomic<int> boundaryCalls{};
      FingeringJobs jobs(
          storage, root,
          [&] {
            int elapsed = 0;
            if (finalRead) {
              auto calls = ++boundaryCalls;
              elapsed = (!beforeCommit || calls >= 2) ? expiry : 0;
            }
            return std::chrono::steady_clock::time_point{} +
                   std::chrono::seconds(elapsed);
          },
          [&](const json &frame) {
            if (frame.value("type", std::string{}) == "result")
              finalRead = true;
          });
      jobs.start("job-song");
      for (int i = 0; i < 1000 && jobs.snapshot()["state"] == "running"; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      CHECK(finalRead);
      CHECK(jobs.snapshot()["state"] == "failed");
      CHECK(storage.song("job-song") == saved);
    }
    std::filesystem::remove_all(folder);
  }
}

TEST_CASE("Ready final result and cancellation cover both decision lock orders",
          "[fingering][fingering-race]") {
  for (bool commitWins : {false, true}) {
    auto folder =
        std::filesystem::temp_directory_path() /
        std::filesystem::path(
            "sr-final-race-" +
            std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
    {
      Storage storage(folder);
      storage.putSong(jobSong(), "");
      storage.editFingering("job-song", {1}, 3);
      auto saved = storage.song("job-song");
      std::promise<void> reached, release;
      auto reachedFuture = reached.get_future();
      auto released = release.get_future().share();
      std::atomic<bool> finalRead{};
      std::atomic<int> calls{};
      FingeringJobs jobs(
          storage, root,
          [&] {
            if (finalRead && ++calls == 2 && commitWins) {
              reached.set_value();
              released.wait_for(std::chrono::seconds(5));
            }
            return std::chrono::steady_clock::now();
          },
          [&](const json &frame) {
            if (frame.value("type", std::string{}) == "result") {
              finalRead = true;
              if (!commitWins) {
                reached.set_value();
                released.wait_for(std::chrono::seconds(5));
              }
            }
          });
      auto job = jobs.start("job-song");
      REQUIRE(reachedFuture.wait_for(std::chrono::seconds(10)) ==
              std::future_status::ready);
      if (!commitWins) {
        jobs.cancel(job["jobId"].get<std::string>());
        release.set_value();
        REQUIRE(jobs.snapshot()["state"] == "cancelled");
        REQUIRE(storage.song("job-song") == saved);
      } else {
        std::atomic<bool> conflict{};
        std::promise<void> cancelling;
        auto started = cancelling.get_future();
        std::thread cancel([&] {
          cancelling.set_value();
          try {
            jobs.cancel(job["jobId"].get<std::string>());
          } catch (const FingeringError &e) {
            conflict = e.status == 409;
          }
        });
        started.wait();
        release.set_value();
        cancel.join();
        REQUIRE(conflict);
        REQUIRE(jobs.snapshot()["state"] == "completed");
        REQUIRE(storage.song("job-song")["fingeringRevision"] == 2);
      }
    }
    std::filesystem::remove_all(folder);
  }
}
