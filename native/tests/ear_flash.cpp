#include <catch2/catch_test_macros.hpp>
#include <sightreading/EarFlash.hpp>
using namespace sightreading;
TEST_CASE("Ear integer validation rejects wide values before narrowing", "[final-validation]") {
 for(const json value:{json(uint64_t{4294967356}),json(UINT64_MAX),json(-1),json(true),json(60.5)}){
  auto c=defaultEarConfig();c["root"]=value;REQUIRE_THROWS(validateEarConfig(c));
 }
 for(int root:{36,84}){auto c=defaultEarConfig();c["root"]=root;REQUIRE_NOTHROW(validateEarConfig(c));}
}
TEST_CASE("Intervals honor selected quality direction transpose order and seed",
          "[ear-flash]") {
  auto c = defaultEarConfig();
  c["intervals"] = {"M3"};
  c["directions"] = {"down"};
  c["root"] = 60;
  c["transpose"] = 2;
  auto a = generateEar(c, 19);
  REQUIRE(a.is_object());
  REQUIRE(a["root"] == 62);
  REQUIRE(a["interval"] == "M3");
  REQUIRE(a["groups"][1][0] == 58);
  REQUIRE(generateEar(c, 19) == a);
  c["intervals"] = {"m2", "P8"};
  REQUIRE(generateEar(c, 1, 1)["interval"] == "P8");
  c["intervals"] = json::array();
  REQUIRE_THROWS(validateEarConfig(c));
  c = defaultEarConfig();
  c["directions"] = json::array();
  REQUIRE_THROWS(validateEarConfig(c));
}
TEST_CASE("Melody groups stay in major key range and direction for all sizes",
          "[ear-flash]") {
  auto c = defaultEarConfig();
  c["mode"] = "melody";
  c["root"] = 60;
  c["range"] = {{"min", 48}, {"max", 84}};
  for (int n = 2; n <= 8; n++)
    for (int chord = 1; chord <= 4; chord++)
      for (auto direction : {"ascending", "descending", "any"}) {
        c["groups"] = n;
        c["pitches"] = chord;
        c["melodyDirection"] = direction;
        auto a = generateEar(c, 18);
        REQUIRE(a.is_object());
        REQUIRE(a["groups"].size() == n);
        int prior = -1;
        for (auto &g : a["groups"]) {
          REQUIRE(g.size() == chord);
          int first = g[0];
          if (prior >= 0 && std::string(direction) != "any")
            REQUIRE((std::string(direction) == "ascending" ? first >= prior
                                                           : first <= prior));
          prior = first;
          for (auto &p : g) {
            int note = p;
            REQUIRE(note >= 48);
            REQUIRE(note <= 84);
            REQUIRE(std::set<int>{0, 2, 4, 5, 7, 9, 11}.contains(note % 12));
          }
        }
      }
  c["range"] = {{"min", 60}, {"max", 61}};
  c["pitches"] = 4;
  REQUIRE_THROWS(generateEar(c, 1));
}
TEST_CASE("Note Math counts letters inclusively and chord spelling retains "
          "inversions",
          "[ear-flash]") {
  auto c = defaultFlashConfig();
  c["roots"] = {"C"};
  c["distances"] = {3};
  auto a = generateFlash(c, 1);
  REQUIRE(a.is_object());
  REQUIRE(a["answer"] == "E");
  c["roots"] = json::array();
  REQUIRE_THROWS(validateFlashConfig(c));
  c = defaultFlashConfig();
  c["mode"] = "chord";
  c["keys"] = {"Gb"};
  c["inversions"] = {2};
  for (unsigned s = 1; s < 40; s++) {
    auto q = generateFlash(c, s);
    REQUIRE(q["inversion"] == 2);
    REQUIRE(q["notes"].size() == 3);
    REQUIRE(q["answer"]["quality"].is_string());
    for (auto &n : q["notes"])
      REQUIRE(n["spelling"]["letter"].is_string());
  }
  c["keys"] = json::array();
  REQUIRE_THROWS(validateFlashConfig(c));
}
TEST_CASE("Examples suppress answers preserve Live and clicks and score fresh "
          "melody chords",
          "[ear-flash]") {
  Engine e({.pianoPath = "missing-test.sfz"});
  EarFlash x(e);
  float out[256]{};
  x.command("ear.configure",
            {{"mode", "melody"}, {"groups", 2}, {"pitches", 2}, {"tempo", 300}},
            0);
  auto target = x.command("ear.next", {{"seed", 8}}, 0)["target"];
  REQUIRE(x.command("ear.replay", json::object(), 0)["playing"] == true);
  REQUIRE_THROWS(
      x.command("ear.answer", {{"pitches", target["groups"][0]}}, 0));
  e.submit({EventType::On, Bus::Live, 1, 0, 60, 90, 1});
  e.command({{"version", 1}, {"id", 1}, {"type", "metronome.start"}});
  e.render(out, 128);
  x.command("ear.stop", json::object(), 128);
  e.render(out, 128);
  REQUIRE(e.snapshot()["heldNotes"].size() == 1);
  REQUIRE(e.snapshot()["metronome"]["running"] == true);
  for (auto &g : target["groups"])
    x.command("ear.answer", {{"pitches", g}}, 256);
  REQUIRE(x.ear(256)["hits"] == 2);
  x.command("ear.replay", json::object(), 256);
  x.command("ear.pause", json::object(), 256);
  REQUIRE(x.ear(256)["paused"] == true);
  x.command("ear.play", json::object(), 256);
  REQUIRE(x.ear(256)["playing"] == true);
  x.command("ear.stop", json::object(), 256);
  e.render(out, 128);
  REQUIRE(e.snapshot()["heldNotes"].size() == 1);
  x.command("flash.configure", {{"roots", {"C"}}, {"distances", {3}}}, 256);
  x.command("flash.next", json::object(), 256);
  x.command("flash.answer", {{"answer", "E"}}, 256);
  REQUIRE(x.flash()["hits"] == 1);
  REQUIRE_THROWS(x.command("flash.answer", {{"answer", "E"}}, 256));
}
TEST_CASE("Native physical answers ignore examples and require fresh complete "
          "chord attacks",
          "[ear-flash]") {
  Engine e({.pianoPath = "missing-test.sfz"});
  EarFlash x(e);
  x.command("ear.configure",
            {{"mode", "melody"}, {"groups", 2}, {"pitches", 2}, {"tempo", 300}},
            0);
  auto target = x.ear(0)["target"];
  x.command("ear.replay", json::object(), 0);
  uint64_t token = 1;
  for (auto &v : target["groups"][0])
    x.input({EventType::On, Bus::Live, 1, 0, uint8_t(v.get<int>()), 90, token++,
             64});
  REQUIRE(x.ear(64)["hits"] == 0);
  x.command("ear.stop", json::object(), 128);
  x.input({EventType::Disconnect, Bus::Live, 1});
  for (auto &g : target["groups"]) {
    for (auto &v : g)
      x.input({EventType::On, Bus::Live, 1, 0, uint8_t(v.get<int>()), 90,
               token++, 256});
    x.input({EventType::Disconnect, Bus::Live, 1});
  }
  REQUIRE(x.ear(256)["hits"] == 2);
  x.command("ear.next", json::object(), 300);
  auto first = x.ear(300)["target"]["groups"][0][0].get<int>();
  x.input({EventType::On, Bus::Live, 1, 0, uint8_t(first), 90, token++, 400});
  x.tick(8000);
  REQUIRE(x.ear(8000)["misses"] == 1);
  x.input({EventType::On, Bus::Live, 1, 0, uint8_t(first), 90, token++, 8100});
  x.input({EventType::Disconnect, Bus::Live, 1});
  x.tick(16000);
  REQUIRE(x.ear(16000)["misses"] == 1);
}
TEST_CASE(
    "Autoplay varies roots and cancellation leaves no future example attacks",
    "[ear-flash-edge]") {
  Engine e({.pianoPath = "missing-test.sfz"});
  EarFlash x(e);
  float out[256]{};
  x.command("ear.configure", {{"intervals", {"M3"}}, {"tempo", 300}}, 0);
  x.command("ear.autoplay", {{"enabled", true}}, 0);
  std::set<int> roots;
  for (int i = 0; i < 8; i++) {
    roots.insert(x.ear(e.sampleFrame())["target"]["root"].get<int>());
    auto end = x.ear(e.sampleFrame())["endFrame"].get<uint64_t>();
    while (e.sampleFrame() < end) {
      x.tick(e.sampleFrame());
      e.render(out, 128);
    }
    x.tick(e.sampleFrame());
  }
  REQUIRE(roots.size() > 1);
  x.command("ear.stop", json::object(), e.sampleFrame());
  for (int i = 0; i < 200; i++) {
    x.tick(e.sampleFrame());
    e.render(out, 128);
  }
  REQUIRE(e.snapshot()["heldNotes"].empty());
  REQUIRE(x.ear(e.sampleFrame())["autoplay"] == false);
}
TEST_CASE(
    "Released attacks and pre-example held keys cannot answer a melody chord",
    "[ear-flash-held]") {
  Engine e({.pianoPath = "missing-test.sfz"});
  EarFlash x(e);
  x.command("ear.configure",
            {{"mode", "melody"}, {"groups", 2}, {"pitches", 2}}, 0);
  auto g = x.ear(0)["target"]["groups"][0];
  int a = g[0], b = g[1];
  x.input({EventType::On, Bus::Live, 1, 0, uint8_t(a), 90, 1, 100});
  x.input({EventType::Off, Bus::Live, 1, 0, uint8_t(a), 0, 1, 200});
  x.input({EventType::On, Bus::Live, 1, 0, uint8_t(b), 90, 2, 300});
  REQUIRE(x.ear(300)["hits"] == 0);
  x.input({EventType::Off, Bus::Live, 1, 0, uint8_t(b), 0, 2, 400});
  x.command("ear.replay", json::object(), 500);
  x.input({EventType::On, Bus::Live, 1, 0, uint8_t(a), 90, 3, 600});
  x.command("ear.stop", json::object(), 700);
  x.input({EventType::On, Bus::Live, 1, 0, uint8_t(a), 90, 3, 800});
  x.input({EventType::On, Bus::Live, 1, 0, uint8_t(b), 90, 4, 800});
  REQUIRE(x.ear(800)["hits"] == 0);
}
TEST_CASE("Opt-in pitch wheel uses centered edge shortcuts and retains live "
          "ownership",
          "[ear-flash-wheel]") {
  Engine e({.pianoPath = "missing-test.sfz"});
  EarFlash x(e);
  x.command("ear.configure", {{"mode", "melody"}, {"pitchWheel", true}}, 0);
  InputEvent bend;
  bend.type = static_cast<EventType>(8);
  bend.bus = Bus::Live;
  bend.source = 1;
  bend.velocity = 110;
  bend.frame = 128;
  auto revision = x.ear(0)["revision"].get<unsigned>();
  x.input(bend);
  REQUIRE(x.ear(128)["playing"] == true);
  auto after = x.ear(128)["revision"];
  REQUIRE(after.get<unsigned>() > revision);
  x.input(bend);
  REQUIRE(x.ear(128)["revision"] == after);
  bend.velocity = 64;
  x.input(bend);
  bend.velocity = 10;
  x.input(bend);
  REQUIRE(x.ear(128)["revision"] != after);
}
TEST_CASE(
    "Pitch wheel pressure preserves release reserve and sounding live owners",
    "[ear-flash-pressure]") {
  EventQueue<32, 8> q;
  InputEvent bend{EventType::PitchWheel, Bus::Live, 1, 0, 0, 110};
  for (int i = 0; i < 24; i++)
    REQUIRE(q.push(bend));
  REQUIRE_FALSE(q.push(bend));
  for (int i = 0; i < 8; i++)
    REQUIRE(q.push({EventType::Off}));
  Engine e({.pianoPath = "missing-test.sfz"});
  float out[256]{};
  REQUIRE(e.submit({EventType::On, Bus::Live, 1, 0, 60, 90, 1}));
  e.render(out, 128);
  unsigned accepted = 0;
  while (e.submit(bend))
    ++accepted;
  REQUIRE(accepted == 3072);
  e.render(out, 128);
  REQUIRE(e.snapshot()["heldNotes"].size() == 1);
}
TEST_CASE(
    "Harmonic reference motif retains a phrase distinct from interval pair",
    "[ear-flash-motif]") {
  Engine e({.pianoPath = "missing-test.sfz"});
  EarFlash x(e);
  x.command("ear.configure", {{"playOrder", "harmonic"}}, 0);
  auto pair = x.command("ear.replay", json::object(), 0);
  auto motif = x.command("ear.motif", json::object(), 0);
  REQUIRE(motif["notes"].size() == 4);
  REQUIRE(motif["endFrame"].get<uint64_t>() > pair["endFrame"].get<uint64_t>());
}
TEST_CASE("Flash answers lock while native examples continue across tabs",
          "[ear-flash-lock]") {
  Engine e({.pianoPath = "missing-test.sfz"});
  EarFlash x(e);
  x.command("flash.configure", {{"roots", {"C"}}, {"distances", {3}}}, 0);
  x.command("ear.play", json::object(), 0);
  REQUIRE_THROWS(x.command("flash.answer", {{"answer", "E"}}, 0));
  x.command("ear.stop", json::object(), 128);
  REQUIRE(x.command("flash.answer", {{"answer", "E"}}, 128)["hits"] == 1);
}
TEST_CASE("Real sampled examples render at exact native frames with "
          "independent lanes",
          "[ear-flash-audio]") {
  struct Trace {
    std::array<InputEvent, 64> events{};
    unsigned count{};
  } trace;
  Engine e(
      {.pianoPath = std::filesystem::path(SR_ROOT) / "native/tests/fixture.sfz",
       .preloadSynchronously = true});
  REQUIRE(e.snapshot()["pianoReady"] == true);
  e.setInputHook(
      [](const InputEvent &event, uint64_t frame, void *context) noexcept {
        auto &t = *static_cast<Trace *>(context);
        if (event.bus == Bus::Example && event.type == EventType::On &&
            t.count < t.events.size()) {
          auto copy = event;
          copy.frame = frame;
          t.events[t.count++] = copy;
        }
      },
      &trace);
  EarFlash x(e);
  x.command("ear.configure",
            {{"intervals", {"M3"}}, {"tempo", 300}, {"root", 60}}, 0);
  e.submit({EventType::Controller, Bus::Playback, SongSource, 0, 7, 0});
  auto example = x.command("ear.replay", json::object(), 0);
  float out[256]{};
  double energy = 0;
  while (e.sampleFrame() < example["endFrame"].get<uint64_t>()) {
    x.tick(e.sampleFrame());
    e.render(out, 128);
    for (float v : out)
      energy += double(v) * v;
  }
  REQUIRE(energy > 0.01);
  REQUIRE(trace.count == 2);
  REQUIRE(trace.events[0].frame ==
          example["notes"][0]["startFrame"].get<uint64_t>());
  REQUIRE(trace.events[1].frame ==
          example["notes"][1]["startFrame"].get<uint64_t>());
  REQUIRE(e.submit({EventType::On, Bus::Live, 1, 0, 60, 90, 900}));
  e.command({{"version", 1}, {"id", 1}, {"type", "metronome.start"}});
  e.render(out, 128);
  x.command("ear.stop", json::object(), e.sampleFrame());
  e.render(out, 128);
  auto s = e.snapshot();
  REQUIRE(s["metronome"]["running"] == true);
  bool live = false;
  for (auto &n : s["heldNotes"])
    live |= n["bus"] == "live" && n["instance"] == 900;
  REQUIRE(live);
}
TEST_CASE("Example start reports bounded queue failure and cancellation "
          "preserves live",
          "[ear-flash-example-pressure]") {
  Engine e({.pianoPath = "missing-test.sfz"});
  EarFlash x(e);
  float out[256]{};
  e.submit({EventType::On, Bus::Live, 1, 0, 60, 90, 99});
  e.render(out, 128);
  x.command("ear.configure", {{"intervals", {"M3"}}}, 128);
  e.render(out, 128);
  for (unsigned i = 0; i < 3072; i++)
    REQUIRE(e.submit(
        {EventType::On, Bus::Live, 2, 0, 64, 90, uint64_t(i + 1), 100000}));
  for (unsigned i = 0; i < 1024; i++)
    REQUIRE(e.submit(
        {EventType::Off, Bus::Live, 2, 0, 64, 0, uint64_t(i + 10000), 100000}));
  REQUIRE_THROWS(x.command("ear.replay", json::object(), e.sampleFrame()));
  REQUIRE(x.ear(e.sampleFrame())["playing"] == false);
  REQUIRE(x.ear(e.sampleFrame())["error"].get<std::string>().find("capacity") !=
          std::string::npos);
  e.render(out, 128);
  auto held = e.snapshot()["heldNotes"];
  REQUIRE(held.size() == 1);
  REQUIRE(held[0]["instance"] == 99);
}
TEST_CASE("Interval study never scores unrelated global live notes",
          "[ear-flash-study]") {
  Engine e({.pianoPath = "missing-test.sfz"});
  EarFlash x(e);
  x.command("ear.configure", {{"mode", "interval"}}, 0);
  x.input({EventType::On, Bus::Live, 1, 0, 60, 90, 1, 128});
  x.tick(10000);
  REQUIRE(x.ear(10000)["misses"] == 0);
  REQUIRE(x.ear(10000)["hits"] == 0);
}
TEST_CASE("Melody mode ignores inherited interval presentation order",
          "[ear-flash-review-order]") {
  for (auto order : {"reverse", "harmonic"}) {
    Engine e({.pianoPath = "missing-test.sfz"});
    EarFlash x(e);
    x.command("ear.configure", {{"mode", "interval"}, {"playOrder", order}}, 0);
    x.command("ear.configure",
              {{"mode", "melody"},
               {"groups", 4},
               {"pitches", 2},
               {"tempo", 300},
               {"melodyDirection", "ascending"}},
              0);
    auto s = x.command("ear.replay", json::object(), 0);
    auto targets = s["target"]["groups"].get<std::vector<std::vector<int>>>();
    std::map<uint64_t, std::vector<int>> byFrame;
    for (auto &n : s["notes"])
      byFrame[n["startFrame"].get<uint64_t>()].push_back(
          n["midiNote"].get<int>());
    std::vector<std::vector<int>> scheduled;
    for (auto &[frame, group] : byFrame)
      scheduled.push_back(group);
    REQUIRE(scheduled == targets);
    x.command("ear.stop", json::object(), 128);
    for (auto &group : scheduled)
      x.command("ear.answer", {{"pitches", group}}, 256);
    REQUIRE(x.ear(256)["hits"] == 4);
    REQUIRE(x.ear(256)["answered"] == true);
  }
}
TEST_CASE("Physical attacks at example end unlock before the next worker tick",
          "[ear-flash-review-end]") {
  for (uint64_t offset : {uint64_t(0), uint64_t(128)}) {
    Engine e({.pianoPath = "missing-test.sfz"});
    EarFlash x(e);
    x.command(
        "ear.configure",
        {{"mode", "melody"}, {"groups", 2}, {"pitches", 1}, {"tempo", 300}}, 0);
    auto s = x.command("ear.replay", json::object(), 0);
    uint64_t end = s["endFrame"];
    int pitch = s["target"]["groups"][0][0];
    x.input({EventType::On, Bus::Live, 1, 0, uint8_t(pitch), 90, 1, end - 2});
    REQUIRE(x.ear(end - 2)["hits"] == 0);
    x.input({EventType::Off, Bus::Live, 1, 0, uint8_t(pitch), 0, 1, end - 1});
    x.input(
        {EventType::On, Bus::Live, 1, 0, uint8_t(pitch), 90, 2, end + offset});
    REQUIRE(x.ear(end + offset)["hits"] == 1);
    REQUIRE(x.ear(end + offset)["playing"] == false);
  }
}
TEST_CASE(
    "Boundary resolution retains autoplay lock and held-before suppression",
    "[ear-flash-review-end]") {
  Engine e({.pianoPath = "missing-test.sfz"});
  EarFlash x(e);
  x.command("ear.configure",
            {{"mode", "melody"}, {"groups", 2}, {"pitches", 1}, {"tempo", 300}},
            0);
  auto s = x.command("ear.replay", json::object(), 0);
  uint64_t end = s["endFrame"];
  int pitch = s["target"]["groups"][0][0];
  x.input({EventType::On, Bus::Live, 1, 0, uint8_t(pitch), 90, 1, end - 1});
  x.input({EventType::On, Bus::Live, 1, 0, uint8_t(pitch), 90, 1, end});
  x.tick(end);
  REQUIRE(x.ear(end)["hits"] == 0);
  x.input({EventType::Disconnect, Bus::Live, 1});
  s = x.command("ear.autoplay", {{"enabled", true}}, end);
  auto oldTarget = s["target"];
  auto nextEnd = s["endFrame"].get<uint64_t>();
  x.input({EventType::On, Bus::Live, 1, 0, uint8_t(pitch), 90, 2, nextEnd});
  REQUIRE(x.ear(nextEnd)["playing"] == true);
  REQUIRE(x.ear(nextEnd)["endFrame"].get<uint64_t>() > nextEnd);
  REQUIRE(x.ear(nextEnd)["hits"] == 0);
}
TEST_CASE("Answer commands resolve example completion at their native frame",
          "[ear-flash-review-command-end]") {
  Engine e({.pianoPath = "missing-test.sfz"});
  EarFlash x(e);
  x.command("ear.configure",
            {{"mode", "melody"}, {"groups", 2}, {"pitches", 1}, {"tempo", 300}},
            0);
  x.command("flash.configure", {{"roots", {"C"}}, {"distances", {3}}}, 0);
  auto s = x.command("ear.replay", json::object(), 0);
  uint64_t end = s["endFrame"];
  REQUIRE_THROWS(x.command("flash.answer", {{"answer", "E"}}, end - 1));
  REQUIRE_THROWS(x.command("ear.answer",
                           {{"pitches", s["target"]["groups"][0]}}, end - 1));
  REQUIRE(x.command("flash.answer", {{"answer", "E"}}, end)["hits"] == 1);
  REQUIRE(x.command("ear.answer", {{"pitches", s["target"]["groups"][0]}},
                    end)["hits"] == 1);
  x.command("flash.next", json::object(), end);
  s = x.command("ear.autoplay", {{"enabled", true}}, end);
  end = s["endFrame"];
  REQUIRE_THROWS(x.command("flash.answer", {{"answer", "E"}}, end));
  REQUIRE(x.ear(end)["endFrame"].get<uint64_t>() > end);
  REQUIRE(x.ear(end)["answerLocked"] == true);
  x.command("ear.pause", json::object(), end);
  REQUIRE_THROWS(x.command("flash.answer", {{"answer", "E"}}, end + 100000));
  REQUIRE(x.ear(end + 100000)["paused"] == true);
}
