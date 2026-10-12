#include <chrono>
#include <csignal>
#include <httplib.h>
#include <iostream>
#include <sightreading/Engine.hpp>
#include <sightreading/Practice.hpp>
#include <sightreading/EarFlash.hpp>
#include <sightreading/Storage.hpp>
#include <sightreading/Transport.hpp>
#include <sightreading/LibraryRoutes.hpp>
#include <sightreading/Tray.hpp>
#include <thread>
#include <unordered_set>
using json = nlohmann::json;
using namespace sightreading;
namespace {
httplib::Server *server{};
std::atomic<bool> stopping{};
void shutdown(int) {
  stopping = true;
  if (server)
    server->stop();
}
uint64_t now() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}
template <class T, size_t N> struct Ring {
  std::array<T, N> data{};
  std::atomic<uint64_t> read{}, write{}, dropped{};
  void push(const T &v) noexcept {
    auto w = write.load(std::memory_order_relaxed);
    if (w - read.load(std::memory_order_acquire) == N) {
      ++dropped;
      return;
    }
    data[w % N] = v;
    write.store(w + 1, std::memory_order_release);
  }
  bool pop(T &v) {
    auto r = read.load();
    if (r == write.load(std::memory_order_acquire))
      return false;
    v = data[r % N];
    read.store(r + 1, std::memory_order_release);
    return true;
  }
};
struct Hooks {
  Ring<InputEvent, 4096> notes;
  Ring<BeatEvent, 1024> beats;
};
} // namespace
int main(int argc, char **argv) {
  try {
    int port = 8765;
    std::filesystem::path dir;
    const char *local = std::getenv("LOCALAPPDATA");
    dir = local ? std::filesystem::path(local) / "SightReading"
                : std::filesystem::path("data");
    std::string web = "web/dist";
    bool tray = true;
    for (int i = 1; i < argc; i++) {
      std::string arg = argv[i];
      if (arg == "--no-tray") { tray = false; continue; }
      if ((arg == "--port" || arg == "--data-dir" || arg == "--web-dir") &&
          i + 1 < argc) {
        std::string value = argv[++i];
        if (arg == "--port")
          port = std::stoi(value);
        else if (arg == "--data-dir")
          dir = value;
        else
          web = value;
      } else
        throw std::invalid_argument("Usage: sightreading [--port 8765] "
                                    "[--data-dir path] [--web-dir path] [--no-tray]");
    }
    if (port < 1 || port > 65535)
      throw std::invalid_argument("Invalid port");
    Storage storage(dir);
    installStarters(storage,"assets/music");
    Hooks hooks;
    EngineOptions options;
    auto savedDisabled = storage.load("disabled");
    if (savedDisabled.is_object())
      for (auto i = savedDisabled.begin(); i != savedDisabled.end(); ++i)
        if (i.value().get<bool>())
          options.disabledInputs.push_back(i.key());
    Engine engine(options);
    Practice practice(options.sampleRate);
    Transport transport(engine);
    EarFlash exercises(engine);
    std::mutex exercisesMutex;
    for(auto pair:{std::pair{"ear","ear.configure"},std::pair{"flash","flash.configure"}}){
      auto saved=storage.load(pair.first);
      if(!saved.is_null())exercises.command(pair.second,saved,engine.sampleFrame());
    }
    std::mutex transportMutex;
    std::mutex practiceMutex;
    auto savedPractice = storage.load("practice");
    if (!savedPractice.is_null())
      practice.configure(savedPractice);
    engine.setInputHook(
        [](const InputEvent &e, uint64_t frame, void *c) noexcept {
          auto event = e;
          event.frame = frame;
          static_cast<Hooks *>(c)->notes.push(event);
        },
        &hooks);
    engine.setBeatHook(
        [](const BeatEvent &e, void *c) noexcept {
          static_cast<Hooks *>(c)->beats.push(e);
        },
        &hooks);
    for (auto pair : {std::pair{"audio", "audio.settings"},
                      std::pair{"metronome", "metronome.config"}}) {
      auto saved = storage.load(pair.first);
      if (!saved.is_null()) {
        auto reply = engine.command({{"version", 1},
                                     {"id", "restore"},
                                     {"type", pair.second},
                                     {"params", saved}});
        if (!reply["ok"].get<bool>())
          throw std::runtime_error(
              "Saved configuration invalid: " +
              reply["error"]["message"].get<std::string>());
      }
    }
    engine.start();
    httplib::Server http;
    server = &http;
    std::signal(SIGINT, shutdown);
    std::signal(SIGTERM, shutdown);
    const std::string host = "127.0.0.1:" + std::to_string(port),
                      origin = "http://" + host;
    const auto generation = now() / 1000000;
    std::mutex commands;
    std::mutex slotsMutex;
    std::array<bool, 32> slots{};
    auto state = [&]() {
      auto s = engine.snapshot();
      s["generation"] = generation;
      s["engineTimeNs"] = now();
      {
        std::lock_guard lock(practiceMutex);
        s["practice"] = practice.snapshot(engine.sampleFrame());
      }
      {std::lock_guard lock(transportMutex);s["transport"]=transport.snapshot(engine.sampleFrame());s["transport"]["engineTimeNs"]=s["engineTimeNs"];}
      {std::lock_guard lock(exercisesMutex);s["ear"]=exercises.ear(engine.sampleFrame());s["flash"]=exercises.flash();}
      return s;

    };
    auto command = [&](const json &req) {
      std::lock_guard lock(commands);
      if (req.is_object() && req.contains("id") &&
          req.value("version", json()) == 1 &&
          req.value("type", json()) == "clock.ping")
        return json{{"version", 1},
                    {"id", req.value("id", json())},
                    {"ok", true},
                    {"result", state()},
                    {"error", nullptr}};
      json reply;
      if(req.is_object()&&req.contains("id")&&req.value("version",json())==1&&req.value("type",json()).is_string()&&(req["type"].get<std::string>().starts_with("ear.")||req["type"].get<std::string>().starts_with("flash."))){
        json result;
        auto type=req["type"].get<std::string>();
        try{std::lock_guard lock(exercisesMutex);result=exercises.command(type,req.value("params",json::object()),engine.sampleFrame());}
        catch(const std::exception& e){return json{{"version",1},{"id",req["id"]},{"ok",false},{"result",nullptr},{"error",{{"code","invalid_exercise"},{"message",e.what()}}}};}
        if(type=="ear.configure"||type=="flash.configure"||type=="ear.option"||type=="flash.option")try{storage.save(type.starts_with("ear.")?"ear":"flash",result["config"]);}catch(const std::exception&){result["accepted"]=true;result["applied"]=true;result["persisted"]=false;return json{{"version",1},{"id",req["id"]},{"ok",false},{"result",result},{"error",{{"code","storage_error"},{"message","Exercise settings accepted but not saved. Resolve local storage and retry saving."}}}};}
        return json{{"version",1},{"id",req["id"]},{"ok",true},{"result",result},{"error",nullptr}};
      }
      if(req.is_object()&&req.contains("id")&&req.value("version",json())==1&&req.value("type",json()).is_string()&&req["type"].get<std::string>().starts_with("transport.")){
        try{std::lock_guard lock(transportMutex);auto type=req["type"].get<std::string>();auto params=req.value("params",json::object());json result;if(type=="transport.load"){auto id=params.at("id").get<std::string>();auto song=storage.song(id);if(song["trashed"]==true)throw std::invalid_argument("Restore this song from trash first.");transport.load(song);storage.patchSong(id,{{"recent",uint64_t(std::chrono::system_clock::now().time_since_epoch().count())}});result=transport.snapshot(engine.sampleFrame());}else result=transport.command(type,params,engine.sampleFrame());return json{{"version",1},{"id",req["id"]},{"ok",true},{"result",result},{"error",nullptr}};}catch(const std::exception &e){return json{{"version",1},{"id",req["id"]},{"ok",false},{"result",nullptr},{"error",{{"code","invalid_transport"},{"message",e.what()}}}};}
      }
      if (req.is_object() && req.contains("id") &&
          req.value("version", json()) == 1 &&
          req.value("type", json()).is_string() &&
          req["type"].get<std::string>().starts_with("practice.")) {
        try {
          std::lock_guard lock(practiceMutex);
          const auto type = req["type"].get<std::string>();
          if (type == "practice.answer") {
            auto answer = req;
            auto action = answer.at("params").at("action").get<std::string>();
            if (action != "on" && action != "off")
              throw std::invalid_argument(
                  "practice.answer action must be on or off.");
            answer["type"] =
                action == "on" ? "virtual.noteOn" : "virtual.noteOff";
            answer["params"].erase("action");
            return engine.command(answer);
          }
          if (type == "practice.configure")
            practice.configure(req.value("params", json::object()));
          else if (type == "practice.option") {
            const auto &params = req.at("params");
            auto field = params.at("field").get<std::string>();
            if ((field != "intervals" && field != "keys") ||
                !params.at("enabled").is_boolean())
              throw std::invalid_argument(
                  "Option toggle requires intervals/keys and boolean enabled.");
            auto values = practice.snapshot(
                engine.sampleFrame())["config"]["generatorOptions"][field];
            auto value = params.at("value");
            auto it = std::find(values.begin(), values.end(), value);
            if (params["enabled"] == true && it == values.end())
              values.push_back(value);
            if (params["enabled"] == false && it != values.end())
              values.erase(it);
            practice.configure({{"generatorOptions", {{field, values}}}});
          } else if (type == "practice.start") {
            const auto frame = engine.sampleFrame();
            engine.followMetronome(practice.snapshot(frame)["config"]["tempo"],
                                   4, 4, 0, frame);
            practice.start(frame);
          } else if (type == "practice.reset") {
            const auto frame = engine.sampleFrame();
            engine.followMetronome(practice.snapshot(frame)["config"]["tempo"],
                                   4, 4, 0, frame);
            practice.reset(frame);
          } else if (type == "practice.stop")
            practice.stop(engine.sampleFrame());
          else if (type == "practice.pause")
            practice.pause(engine.sampleFrame());
          else if (type == "practice.resume")
            practice.resume(engine.sampleFrame());
          else
            throw std::invalid_argument("Unknown practice command.");
          reply = {{"version", 1},
                   {"id", req["id"]},
                   {"ok", true},
                   {"result", practice.snapshot(engine.sampleFrame())},
                   {"error", nullptr}};
        } catch (const std::exception &e) {
          reply = {
              {"version", 1},
              {"id", req["id"]},
              {"ok", false},
              {"result", nullptr},
              {"error", {{"code", "invalid_practice"}, {"message", e.what()}}}};
        }
      } else
        reply = engine.command(req);
      if (reply["ok"].get<bool>()) {
        auto type = req["type"].get<std::string>();
        auto s = engine.snapshot();
        try {
          if (type == "audio.settings" || type == "audio.channel")
            storage.save("audio", s["pendingChange"].is_null()
                                      ? s["settings"]
                                      : s["pendingChange"]);
          if (type == "metronome.config" || type == "metronome.meter" ||
              type == "metronome.accent")
            storage.save("metronome", s["metronome"]["settings"]);
          if (type == "devices.enable")
            storage.save("disabled", s["disabledInputs"]);
          if (type == "practice.configure" || type == "practice.option") {
            std::lock_guard lock(practiceMutex);
            storage.save("practice",
                         practice.snapshot(engine.sampleFrame())["config"]);
          }
        } catch (const std::exception &e) {
          auto result = reply["result"];
          if (!result.is_object())
            result = json::object();
          result["accepted"] = true;
          result["applied"] = !result.value("deferred", false);
          result["persisted"] = false;
          return json{{"version", 1},
                      {"id", reply["id"]},
                      {"ok", false},
                      {"result", result},
                      {"error",
                       {{"code", "storage_error"},
                        {"message",
                         std::string("Native change accepted but could not be "
                                     "saved; it may be lost on restart. Retry "
                                     "saving after resolving local storage: ") +
                             e.what()}}}};
        }
      }
      return reply;
    };
    http.set_write_timeout(2);
    http.set_payload_max_length(20 * 1024 * 1024 + 64 * 1024);
    http.set_pre_routing_handler([&](const httplib::Request &req,
                                     httplib::Response &res) {
      bool mutation = req.method != "GET" && req.method != "HEAD";
      bool upgrade = req.path == "/api/v1/events";
      auto o = req.get_header_value("Origin");
      if (req.get_header_value_count("Host") != 1 ||
          req.get_header_value_count("Origin") > 1 ||
          req.get_header_value("Host") != host || (!o.empty() && o != origin) ||
          ((mutation || upgrade) && o != origin)) {
        res.status = 403;
        res.set_content("Forbidden local origin", "text/plain");
        return httplib::Server::HandlerResponse::Handled;
      }
      res.set_header("X-Content-Type-Options", "nosniff");
      return httplib::Server::HandlerResponse::Unhandled;
    });
    auto output = [](httplib::Response &res, const json &value) {
      res.set_content(value.dump(), "application/json");
      res.set_header("Cache-Control", "no-store");
    };
    http.Post("/api/v1/shutdown", [&](auto &, auto &res) {
      output(res, {{"stopping", true}}); stopping = true;
    });
    http.Get("/api/v1/status",
             [&](auto &, auto &res) { output(res, state()); });
    http.Get("/api/v1/devices", [&](auto &, auto &res) {
      auto s = state();
      output(res, {{"inputs", s["devices"]},
                   {"midiOutputs", s["midiOutputs"]},
                   {"audioOutputs", s["audioOutputs"]}});
    });
    http.Get("/api/v1/settings",
             [&](auto &, auto &res) { output(res, state()["settings"]); });
    http.Get("/api/v1/metronome",
             [&](auto &, auto &res) { output(res, state()["metronome"]); });
    auto post = [&](const httplib::Request &req, httplib::Response &res) {
      try {
        auto reply = command(json::parse(req.body));
        if (!reply["ok"].get<bool>() &&
            reply["error"]["code"] == "storage_error")
          res.status = 500;
        output(res, reply);
      } catch (const json::parse_error &) {
        res.status = 400;
        output(res, {{"version", 1},
                     {"id", nullptr},
                     {"ok", false},
                     {"result", nullptr},
                     {"error",
                      {{"code", "invalid_json"},
                       {"message", "Expected a JSON command"}}}});
      }
    };
    http.Post("/api/v1/command", post);
    http.Put("/api/v1/settings", [&](const auto &req, auto &res) {
      try {
        json reply = command({{"version", 1},
                              {"id", "settings"},
                              {"type", "audio.settings"},
                              {"params", json::parse(req.body)}});
        if (!reply["ok"].get<bool>() &&
            reply["error"]["code"] == "storage_error")
          res.status = 500;
        output(res, reply);
      } catch (const json::parse_error &) {
        res.status = 400;
        output(res, {{"version", 1},
                     {"id", "settings"},
                     {"ok", false},
                     {"result", nullptr},
                     {"error",
                      {{"code", "invalid_json"},
                       {"message", "Invalid settings JSON"}}}});
      }
    });
    libraryRoutes(http,storage,engine.sampleRate());
    // One control-worker drains hooks; per-connection snapshots include bounded
    // event history.
    std::mutex eventMutex;
    json events = json::array();
    std::atomic<bool> pumping{true};
    uint64_t sequence{};
    uint64_t practiceRevision = UINT64_MAX;
    uint64_t practiceDrops{};
    std::thread pump([&] {
      while (pumping) {
        json additions = json::array();
        InputEvent e;
        BeatEvent b;
        const auto tickFrame = engine.sampleFrame();
        while (hooks.notes.pop(e)) {
          {
            std::lock_guard lock(practiceMutex);
            practice.input(e);
          }
          {std::lock_guard lock(transportMutex);transport.input(e);}
          {std::lock_guard lock(exercisesMutex);exercises.input(e);}
          additions.push_back({{"type", "note"},
                               {"event",
                                {{"kind", unsigned(e.type)},
                                 {"bus", unsigned(e.bus)},
                                 {"sourceId", e.source},
                                 {"channel", e.channel},
                                 {"note", e.note},
                                 {"velocity", e.velocity},
                                 {"instance", e.instance},
                                 {"sampleFrame", e.frame}}}});
        }
        {
          std::lock_guard lock(practiceMutex);
          const auto drops = hooks.notes.dropped.load();
          if (drops != practiceDrops) {
            practiceDrops = drops;
            practice.recoverHeld(engine.snapshot()["heldNotes"], tickFrame);
            {std::lock_guard lock(transportMutex);transport.recover(tickFrame);}
            {std::lock_guard lock(exercisesMutex);exercises.recoverInput();}
          }
          practice.tick(tickFrame);
          if (practice.revision() != practiceRevision) {
            auto p = practice.snapshot(tickFrame);
            practiceRevision = p["revision"];
            additions.push_back({{"type", "practice"},
                                 {"event",
                                  {{"sampleFrame", tickFrame},
                                   {"revision", practiceRevision},
                                   {"index", p["index"]},
                                   {"hits", p["hits"]},
                                   {"misses", p["misses"]},
                                   {"running", p["running"]}}}});
          }
        }
        {std::lock_guard lock(transportMutex);transport.tick(tickFrame);}
        {std::lock_guard lock(exercisesMutex);exercises.tick(tickFrame);}
        while (hooks.beats.pop(b))

          additions.push_back({{"type", "beat"},
                               {"event",
                                {{"sampleFrame", b.sampleFrame},
                                 {"bar", b.bar},
                                 {"beat", b.beat},
                                 {"subdivision", b.subdivision},
                                 {"countIn", b.countIn}}}});
        {
          std::lock_guard lock(eventMutex);
          for (auto &v : additions) {
            v["sequence"] = ++sequence;
            events.push_back(v);
          }
          while (events.size() > 512)
            events.erase(events.begin());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    });
    http.WebSocket("/api/v1/events", [&](const auto &,
                                         httplib::ws::WebSocket &ws) {
      ws.set_read_timeout(std::chrono::milliseconds(5));
      unsigned slot = 32;
      {
        std::lock_guard lock(slotsMutex);
        for (unsigned i = 0; i < 32; i++)
          if (!slots[i]) {
            slots[i] = true;
            slot = i;
            break;
          }
      }
      if (slot == 32) {
        ws.close(httplib::ws::CloseStatus::PolicyViolation,
                 "Too many browser connections");
        return;
      }
      const std::string ownedSource = "browser:slot:" + std::to_string(slot);
      std::unordered_set<std::string> sources;
      uint64_t lastSequence = 0;
      bool firstSnapshot = true;
      auto next = std::chrono::steady_clock::now();
      std::string message;
      while (ws.is_open() && !stopping.load()) {
        if (std::chrono::steady_clock::now() >= next) {
          json batch = json::array();
          bool resync = firstSnapshot;
          {
            std::lock_guard lock(eventMutex);
            if (!events.empty() &&
                events.front()["sequence"].get<uint64_t>() > lastSequence + 1)
              resync = true;
            for (auto &v : events)
              if (v["sequence"].get<uint64_t>() > lastSequence)
                batch.push_back(v);
            lastSequence = sequence;
          }
          if (!ws.send(json{{"version", 1},
                            {"type", "snapshot"},
                            {"generation", generation},
                            {"state", state()},
                            {"events", batch},
                            {"resync", resync},
                            {"eventDrops", hooks.notes.dropped.load() +
                                               hooks.beats.dropped.load()}}
                           .dump()))
            break;
          firstSnapshot = false;
          next += std::chrono::microseconds(33333);
          if (next < std::chrono::steady_clock::now())
            next = std::chrono::steady_clock::now();
        }
        auto result = ws.read(message);
        if (result == httplib::ws::Fail)
          break;
        if (result == httplib::ws::Timeout)
          continue;
        if (result != httplib::ws::Text)
          continue;
        try {
          auto req = json::parse(message);
          if (req.is_object() && req.contains("params") &&
              req["params"].is_object() && req["params"].contains("source") &&
              req["params"]["source"].is_string()) {
            auto source = req["params"]["source"].get<std::string>();
            if (!source.starts_with("midi:")) {
              req["params"]["source"] = ownedSource;
              sources.insert(ownedSource);
            }
          }
          ws.send(command(req).dump());
        } catch (const json::parse_error &) {
          ws.send(json{
              {"version", 1},
              {"id", nullptr},
              {"ok", false},
              {"result", nullptr},
              {"error",
               {{"code", "invalid_json"}, {"message", "Invalid command JSON"}}}}
                      .dump());
        }
      }
      for (auto &source : sources)
        engine.command({{"version", 1},
                        {"id", "disconnect"},
                        {"type", "virtual.disconnect"},
                        {"params", {{"source", source}}}});
      {
        std::lock_guard lock(slotsMutex);
        slots[slot] = false;
      }
    });
    if (std::filesystem::exists(web))
      http.set_mount_point("/", web);
    http.set_error_handler([&](const auto &req, auto &res) {
      if (res.status == 404 && req.method == "GET" &&
          !req.path.starts_with("/api/") &&
          std::filesystem::exists(web + "/index.html")) {
        res.status = 200;
        res.set_file_content(web + "/index.html");
      }
    });
    std::cout << "Sight Reading at " << origin << std::endl;
    std::thread trayThread;
    if (tray) trayThread = std::thread([&] { runTray(origin, stopping); });
    std::jthread lifecycle([&](std::stop_token token) {
      while (!token.stop_requested() && !stopping.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      if (stopping.load()) http.stop();
    });
    bool listened = http.listen("127.0.0.1", port);
    stopping = true;
    lifecycle.request_stop();
    if (trayThread.joinable()) trayThread.join();
    pumping = false;
    pump.join();
    engine.stop();
    return listened ? 0 : 1;
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
}
