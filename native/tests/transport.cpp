#include <catch2/catch_test_macros.hpp>
#include <sightreading/Transport.hpp>
using namespace sightreading;
using json = nlohmann::json;
TEST_CASE("Song windows index late dense timelines without losing spanning notes", "[window]") {
  json song={{"ppq",480},{"tracks",json::array({{{"staff","split"}}})},{"tempoMap",json::array({{{"tick",0},{"microseconds",500000}}})},{"notes",json::array()}};
  for(int i=0;i<10000;i++)song["notes"].push_back({{"id",i},{"trackId",0},{"midiNote",60},{"startTick",i*48},{"endTick",i*48+24}});
  song["notes"][0]["endTick"]=480000;
  SongWindow window(song);
  auto result=window.query(470000,470200,48000,4096);
  REQUIRE(result[0]["id"]==0);
  REQUIRE(result.size()==5);
  REQUIRE(window.inspected()<100);
  REQUIRE(window.query(470000,470200,48000,2).size()==2);
}
static json model() {
  return {{"id", "test"},
          {"title", "Test"},
          {"ppq", 480},
          {"durationTicks", 1920},
          {"durationSeconds", 2.0},
          {"tracks", json::array({{{"id", 0},
                                   {"practice", true},
                                   {"backing", false},
                                   {"staff", "split"}},
                                  {{"id", 1},
                                   {"practice", false},
                                   {"backing", true},
                                   {"staff", "bass"}}})},
          {"notes", json::array({{{"id", 1},
                                  {"trackId", 0},
                                  {"channel", 0},
                                  {"midiNote", 60},
                                  {"velocity", 90},
                                  {"startTick", 480},
                                  {"endTick", 960}},
                                 {{"id", 2},
                                  {"trackId", 1},
                                  {"channel", 1},
                                  {"midiNote", 48},
                                  {"velocity", 80},
                                  {"startTick", 0},
                                  {"endTick", 1440}}})},
          {"controllers", json::array({{{"trackId", 1},
                                        {"tick", 0},
                                        {"channel", 1},
                                        {"controller", 64},
                                        {"value", 127}},
                                       {{"trackId", 1},
                                        {"tick", 1600},
                                        {"channel", 1},
                                        {"controller", 64},
                                        {"value", 0}}})},
          {"tempoMap", json::array({{{"tick", 0}, {"microseconds", 500000}}})},
          {"meterMap",
           json::array({{{"tick", 0}, {"numerator", 4}, {"denominator", 4}}})},
          {"keyMap", json::array()}};
}
TEST_CASE("Score dragging seeks seconds through the native tempo map", "[song-drag]") {
  Engine e;
  Transport t(e);
  auto s = model();
  s["tempoMap"].push_back({{"tick", 960}, {"microseconds", 1000000}});
  s["durationSeconds"] = 3.0;
  t.load(s);
  t.command("transport.seek", {{"seconds", 2.0}}, 0);
  REQUIRE(t.snapshot(0)["songTick"] == 1440);
  REQUIRE(t.snapshot(0)["songSeconds"] == 2.0);
  REQUIRE_THROWS(t.command("transport.seek", {{"seconds", -1}}, 0));
  REQUIRE_THROWS(t.command("transport.seek", {{"seconds", 4}}, 0));
  REQUIRE(t.snapshot(0)["songTick"] == 1440);
}
TEST_CASE("Selected passage play and restart start at A and preserve options", "[transport][selection]") {
  Engine e;
  Transport t(e);
  t.load(model());
  t.command("transport.configure", {{"loop", true}, {"a", 480}, {"b", 960},
    {"speed", .5}, {"hand", "right"}, {"hints", true}}, 0);
  t.command("transport.play", {}, 0);
  REQUIRE(t.snapshot(256)["songTick"] == 480);
  t.command("transport.seek", {{"tick", 720}}, 256);
  t.command("transport.restart", {}, 512);
  auto p = t.snapshot(768);
  REQUIRE(p["songTick"] == 480);
  REQUIRE(p["config"]["speed"] == .5);
  REQUIRE(p["config"]["hand"] == "right");
  REQUIRE(p["config"]["hints"] == true);
  auto edited = model();
  edited["tracks"][0]["backing"] = true;
  t.load(edited);
  REQUIRE(t.snapshot(768)["config"]["a"] == 480);
  REQUIRE(t.snapshot(768)["config"]["b"] == 960);
  t.command("transport.stop", {}, 768);
  t.command("transport.play", {}, 768);
  REQUIRE(t.snapshot(1024)["songTick"] == 480);
  t.command("transport.configure", {{"loop", false}}, 1024);
  t.command("transport.restart", {}, 1024);
  REQUIRE(t.snapshot(1280)["songTick"] == 0);
}
TEST_CASE("Passage returns animate immediately for two seconds before replay", "[return]") {
 Engine e; Transport t(e); t.load(model());
 t.command("transport.configure",{{"loop",true},{"a",480},{"b",960},{"hints",true}},0);
 t.command("transport.play",{},0);
 t.tick(24256);
 auto end=t.snapshot(24256);
 REQUIRE(end["state"]=="returning");
 REQUIRE(end["songTick"]==960);
 t.tick(48256);
 REQUIRE(t.snapshot(48256)["songTick"].get<double>()<960);
 REQUIRE(t.snapshot(48256)["songTick"].get<double>()>480);
 t.input({EventType::On,Bus::Live,1,0,65,90,1,48256});
 REQUIRE(t.snapshot(48256)["misses"]==end["misses"]);
 REQUIRE(t.snapshot(48256)["target"].empty());
 REQUIRE(t.snapshot(48256)["hintKeys"].empty());
 t.tick(72256);
 REQUIRE(t.snapshot(72256)["songTick"]==720);
 t.tick(120256);
 REQUIRE(t.snapshot(120256)["state"]=="playing");
 REQUIRE(t.snapshot(120256)["songTick"]==480);
 t.command("transport.pause",{},120256);
 t.command("transport.seek",{{"tick",900}},120256);
 t.command("transport.return",{},120256);
 REQUIRE(t.snapshot(168256)["songTick"]==690);
 t.tick(216256);
 REQUIRE(t.snapshot(216256)["state"]=="paused");
 REQUIRE(t.snapshot(216256)["songTick"]==480);
 t.command("transport.return",{},216256);
 t.command("transport.stop",{},220256);
 t.tick(400000);
 REQUIRE(t.snapshot(400000)["state"]=="stopped");
 REQUIRE(t.snapshot(400000)["songTick"]==0);
}
TEST_CASE("Wait resumes at A after return and pauses safely mid-animation", "[return]") {
 Engine e;Transport t(e);t.load(model());
 t.command("transport.configure",{{"mode","wait"},{"loop",true},{"a",480},{"b",960}},0);
 t.command("transport.play",{},0);t.tick(256);
 REQUIRE(t.snapshot(256)["waiting"]==true);
 t.input({EventType::On,Bus::Live,1,0,60,90,1,256});
 t.tick(24512);
 REQUIRE(t.snapshot(24512)["state"]=="returning");
 t.tick(120512);t.tick(120768);
 REQUIRE(t.snapshot(120768)["waiting"]==true);
 REQUIRE(t.snapshot(120768)["songTick"]==480);
 t.command("transport.return",{},120768);
 t.command("transport.pause",{},144768);
 REQUIRE(t.snapshot(144768)["state"]=="paused");
 t.tick(240000);
 REQUIRE(t.snapshot(240000)["state"]=="paused");
}
TEST_CASE("Passage scoring and hints exclude targets beyond B in every mode", "[selection]") {
  for (auto mode : {"continuous", "pauseOnMiss", "wait"}) {
    Engine e;
    Transport t(e);
    auto s = model();
    auto next = s["notes"][0];
    next["id"] = 3;
    next["midiNote"] = 64;
    next["startTick"] = 960;
    next["endTick"] = 1440;
    s["notes"].push_back(next);
    t.load(s);
    t.command("transport.configure", {{"loop", true}, {"a", 480}, {"b", 900},
      {"mode", mode}, {"hints", true}}, 0);
    t.command("transport.play", {}, 0);
    t.tick(256);
    t.input({EventType::On, Bus::Live, 1, 0, 60, 90, 1, 256});
    t.input({EventType::Off, Bus::Live, 1, 0, 60, 0, 1, 257});
    REQUIRE(t.snapshot(20656)["hits"] == 1);
    // This attack is within normal tolerance of the excluded next note.
    t.input({EventType::On, Bus::Live, 1, 0, 64, 90, 2, 20656});
    t.input({EventType::On, Bus::Live, 1, 0, 65, 90, 3, 20657});
    auto p = t.snapshot(20657);
    REQUIRE(p["hits"] == 1);
    REQUIRE(p["misses"] == 0);
    REQUIRE(p["retry"] == false);
    REQUIRE(p["target"].empty());
    REQUIRE(p["hintKeys"].empty());
    REQUIRE(p["songTick"].get<double>() < 900);
  }
}
TEST_CASE("Wait mode freezes at the selected hand and continues only after correct input", "[song-wait]") {
  for (auto hand : {"left", "right", "both"}) {
    Engine e;
    Transport t(e);
    auto s = model();
    s["tracks"][1]["practice"] = true;
    s["notes"][1]["startTick"] = 480;
    t.load(s);
    t.command("transport.configure", {{"mode", "wait"}, {"hand", hand}, {"hints", true}}, 0);
    t.command("transport.play", {}, 0);
    t.tick(24256);
    auto p = t.snapshot(24256);
    REQUIRE(p["waiting"] == true);
    REQUIRE(p["songTick"] == 480);
    REQUIRE(p["misses"] == 0);
    REQUIRE(p["hintKeys"].size() == (std::string(hand) == "both" ? 2 : 1));
    t.tick(100000);
    REQUIRE(t.snapshot(100000)["songTick"] == 480);
    t.input({EventType::On, Bus::Live, 1, 0, 70, 90, 1, 100000});
    REQUIRE(t.snapshot(100000)["waiting"] == true);
    if (std::string(hand) != "right")
      t.input({EventType::On, Bus::Live, 1, 0, 48, 90, 2, 100100});
    if (std::string(hand) == "both") REQUIRE(t.snapshot(100100)["waiting"] == true);
    if (std::string(hand) != "left")
      t.input({EventType::On, Bus::Live, 1, 0, 60, 90, 3, 100200});
    REQUIRE(t.snapshot(100200)["waiting"] == false);
    REQUIRE(t.snapshot(100200)["state"] == "playing");
    REQUIRE(t.snapshot(100200)["hits"] == 1);
  }
}
TEST_CASE("Hints are independent of wait and disabling wait resumes a frozen song", "[song-wait]") {
  Engine e;
  Transport t(e);
  t.load(model());
  t.command("transport.configure", {{"mode", "wait"}, {"hand", "right"}}, 0);
  t.command("transport.play", {}, 0);
  t.tick(24256);
  REQUIRE(t.snapshot(24256)["waiting"] == true);
  REQUIRE(t.snapshot(24256)["hintKeys"].empty());
  t.command("transport.configure", {{"hints", true}}, 24256);
  t.tick(24512);
  REQUIRE(t.snapshot(24512)["waiting"] == true);
  REQUIRE(t.snapshot(24512)["hintKeys"][0]["hand"] == "right");
  t.command("transport.configure", {{"mode", "continuous"}}, 24512);
  REQUIRE(t.snapshot(24512)["state"] == "playing");
  REQUIRE(t.snapshot(24512)["waiting"] == false);
  REQUIRE_FALSE(t.snapshot(24512)["hintKeys"].empty());
}
TEST_CASE("Hint colors honor explicit staff and seek clears stale targets", "[song-wait]") {
  Engine e;
  Transport t(e);
  auto s = model();
  s["tracks"][0]["staff"] = "bass";
  s["tracks"][1]["backing"] = false;
  t.load(s);
  t.command("transport.configure", {{"hints", true}, {"hand", "left"}}, 0);
  REQUIRE(t.snapshot(0)["hintKeys"][0]["midiNote"] == 60);
  REQUIRE(t.snapshot(0)["hintKeys"][0]["hand"] == "left");
  t.command("transport.seek", {{"tick", 1500}}, 0);
  REQUIRE(t.snapshot(0)["hintKeys"].empty());
}
TEST_CASE("Selected backing hand supplies wait and hint targets", "[song-wait]") {
  for (bool reverse : {false, true}) {
  for (auto hand : {"left", "right", "both"}) {
    Engine e;
    Transport t(e);
    auto s = model(); // Bass is backing-only, matching imported starter songs.
    if (reverse) {
      s["tracks"][0]["practice"] = false;
      s["tracks"][0]["backing"] = true;
      s["tracks"][1]["practice"] = true;
      s["tracks"][1]["backing"] = false;
    }
    s["notes"][0]["startTick"] = 0;
    t.load(s);
    t.command("transport.configure", {{"mode", "wait"}, {"hand", hand}, {"hints", true}}, 0);
    t.command("transport.play", {}, 0);
    t.tick(256);
    auto p = t.snapshot(256);
    REQUIRE(p["waiting"] == true);
    REQUIRE(p["target"].size() == (std::string(hand) == "both" ? 2 : 1));
    REQUIRE(p["hintKeys"].size() == p["target"].size());
    if (std::string(hand) != "right") REQUIRE(p["hintKeys"][0]["hand"] == "left");
    t.command("transport.configure", {{"mode", "continuous"}}, 256);
    REQUIRE_FALSE(t.snapshot(256)["hintKeys"].empty());
  }
  }
}
TEST_CASE("Transport hand scoring follows explicit staff before split pitch", "[transport][hand]") {
  Engine e;
  Transport t(e);
  auto song = model();
  song["tracks"][1]["practice"] = true;
  song["tracks"][0]["staff"] = "bass";
  song["tracks"][1]["staff"] = "treble";
  for (auto &note : song["notes"]) note["startTick"] = 480;
  t.load(song);
  REQUIRE(t.snapshot(0)["config"]["hand"] == "both");
  t.command("transport.configure", {{"hand", "left"}}, 0);
  REQUIRE(t.snapshot(0)["target"] == json::array({60}));
  t.command("transport.configure", {{"hand", "right"}}, 0);
  REQUIRE(t.snapshot(0)["target"] == json::array({48}));
  auto before = t.snapshot(0);
  REQUIRE_THROWS(t.command("transport.configure", {{"hand", "feet"}}, 0));
  REQUIRE(t.snapshot(0) == before);
  song["tracks"][0]["staff"] = "grand";
  song["tracks"][1]["staff"] = "split";
  t.load(song);
  REQUIRE(t.snapshot(0)["target"] == json::array({60}));
  t.command("transport.configure", {{"hand", "left"}}, 0);
  REQUIRE(t.snapshot(0)["target"] == json::array({48}));
}
TEST_CASE("Windowed measures honor tempo meter changes and fractional bar ticks", "[window][measures]") {
  auto song = model();
  song["durationTicks"] = 10000;
  song["meterMap"] = json::array({{{"tick", 0}, {"numerator", 4}, {"denominator", 4}},
                                 {{"tick", 2400}, {"numerator", 3}, {"denominator", 8}}});
  song["tempoMap"].push_back({{"tick", 1920}, {"microseconds", 1000000}});
  SongWindow window(song);
  auto bars = window.measures(1800, 3200, 48000);
  REQUIRE(bars == json::array({{{"number", 2}, {"startFrame", 96000}},
                              {{"number", 3}, {"startFrame", 144000}},
                              {{"number", 4}, {"startFrame", 216000}}}));
  song["ppq"] = 101;
  song["durationTicks"] = 1000000;
  song["meterMap"] = json::array({{{"tick", 0}, {"numerator", 1}, {"denominator", 64}}});
  song["tempoMap"] = json::array({{{"tick", 0}, {"microseconds", 500000}}});
  SongWindow fractional(song);
  auto late = fractional.measures(6300, 6326, 48000);
  REQUIRE(late.size() == 4);
  REQUIRE(late[0]["number"] == 1000);
  REQUIRE(late[0]["startFrame"] == 1498500);
  REQUIRE_THROWS(fractional.measures(0, 1000000, 48000));
  REQUIRE_THROWS(fractional.measures(-1, 1, 48000));
}
TEST_CASE("Hand scoring changes retain playback audio and global live ownership", "[transport][hand]") {
  Engine e;
  Transport t(e);
  t.load(model());
  e.submit({EventType::On, Bus::Live, 1, 0, 72, 90, 100, 0});
  std::vector<float> audio(48000);
  e.render(audio.data(), 128);
  t.command("transport.configure", {{"hand", "left"}}, 0);
  t.command("transport.play", {}, 0);
  t.tick(0);
  e.render(audio.data(), 24000);
  t.tick(24000);
  e.render(audio.data(), 512);
  bool live = false, melody = false, backing = false;
  auto held = e.snapshot()["heldNotes"];
  for (auto note : held) {
    live |= note["note"] == 72;
    melody |= note["note"] == 60;
    backing |= note["note"] == 48;
  }
  REQUIRE(live);
  REQUIRE(melody);
  REQUIRE(backing);
  REQUIRE(t.snapshot(24512)["target"].empty());
}
TEST_CASE("Transport native play seek cancel ownership and score use physical "
          "attacks",
          "[transport]") {
  Engine e;
  Transport t(e);
  t.load(model());
  t.command("transport.play", {}, 0);
  t.tick(0);
  std::vector<float> audio(1024);
  e.render(audio.data(), 512);
  auto held = e.snapshot()["heldNotes"];
  REQUIRE(held.size() == 1);
  REQUIRE(held[0]["note"] == 48);
  REQUIRE(t.snapshot(512)["state"] == "playing");
  REQUIRE(t.events(0, 1000).size() == 2);
  e.submit({EventType::On, Bus::Live, 1, 0, 72, 90, 100, 0});
  e.render(audio.data(), 128);
  t.command("transport.seek", {{"tick", 720}}, 640);
  t.tick(640);
  e.render(audio.data(), 512);
  REQUIRE(t.snapshot(1152)["channelControllers"]["1"]["64"] == 127);
  t.command("transport.stop", {}, 1152);
  e.render(audio.data(), 128);
  REQUIRE(e.snapshot()["heldNotes"].size() == 1);
  REQUIRE(e.snapshot()["heldNotes"][0]["note"] == 72);
}
TEST_CASE("Default playback sounds practice melody and backing together", "[transport]") {
  Engine e;
  Transport t(e);
  t.load(model());
  t.command("transport.play", {}, 0);
  t.tick(0);
  std::vector<float> audio(e.sampleRate());
  e.render(audio.data(), e.sampleRate() / 2);
  t.tick(e.sampleRate() / 2);
  e.render(audio.data(), 512);
  bool melody = false, backing = false;
  const auto held = e.snapshot()["heldNotes"];
  for (const auto &note : held) {
    melody |= note["note"] == 60;
    backing |= note["note"] == 48;
  }
  REQUIRE(melody);
  REQUIRE(backing);
}
TEST_CASE("Accompaniment weighted inference stays silent below seventy percent",
          "[transport]") {
  auto s = model();
  s["notes"] = json::array();
  unsigned id = 0;
  for (unsigned pitch : {60, 64, 67})
    s["notes"].push_back({{"id", ++id},
                          {"trackId", 0},
                          {"channel", 0},
                          {"midiNote", pitch},
                          {"velocity", 90},
                          {"startTick", 0},
                          {"endTick", 1920}});
  REQUIRE(inferChords(s)[0]["root"] == 0);
  REQUIRE(inferChords(s)[0]["confidence"].get<double>() >= 0.7);
  for (unsigned pitch : {61, 62, 63, 65, 66, 68, 69, 70, 71})
    s["notes"].push_back({{"id", ++id},
                          {"trackId", 0},
                          {"channel", 0},
                          {"midiNote", pitch},
                          {"velocity", 90},
                          {"startTick", 0},
                          {"endTick", 1920}});
  REQUIRE(inferChords(s)[0]["pitches"].empty());
}
TEST_CASE("Playback channel gain curves change sounding voices at exact sample "
          "without live bleed",
          "[transport][controllers]") {
  EngineOptions options;
  options.pianoPath = std::string(SR_ROOT) + "/native/tests/fixture.sfz";
  options.preloadSynchronously = true;
  Engine e(options);
  e.submit({EventType::On, Bus::Playback, 126, 3, 60, 100, 1});
  std::vector<float> audio(48000 * 2);
  e.render(audio.data(), 4800);
  uint64_t at = e.sampleFrame() + 64;
  e.submit({EventType::Controller, Bus::Playback, 126, 3, 7, 0, 0, at});
  e.render(audio.data(), 128);
  double before = 0, after = 0;
  for (unsigned i = 0; i < 64 * 2; i++)
    before += std::abs(audio[i]);
  for (unsigned i = 64 * 2; i < 128 * 2; i++)
    after += std::abs(audio[i]);
  REQUIRE(before > 0.001);
  REQUIRE(after < 0.00001);
  e.submit({EventType::Controller, Bus::Playback, 126, 3, 7, 127});
  e.submit({EventType::Controller, Bus::Playback, 126, 3, 10, 0});
  e.render(audio.data(), 128);
  double left = 0, right = 0;
  for (unsigned i = 0; i < 128; i++) {
    left += std::abs(audio[i * 2]);
    right += std::abs(audio[i * 2 + 1]);
  }
  REQUIRE(left > .001);
  REQUIRE(right < .00001);
  e.submit({EventType::On, Bus::Live, 1, 3, 64, 100, 10});
  e.submit({EventType::Controller, Bus::Playback, 126, 3, 11, 0});
  e.render(audio.data(), 4800);
  REQUIRE(e.snapshot()["heldNotes"].size() == 2);
  double live = 0;
  for (unsigned i = 0; i < 9600; i++)
    live += std::abs(audio[i]);
  REQUIRE(live > .01);
}
TEST_CASE("Empty transport snapshot and loop position are safe before load and "
          "across lookahead",
          "[transport]") {
  Engine e;
  Transport t(e);
  REQUIRE(t.snapshot(0)["songId"] == "");
  t.load(model());
  t.command("transport.configure", {{"loop", true}, {"a", 480}, {"b", 960}}, 0);
  t.command("transport.seek", {{"tick", 480}}, 0);
  t.command("transport.play", {}, 0);
  t.tick(23000);
  REQUIRE(t.snapshot(23000)["songTick"].get<double>() > 900);
  t.tick(25000);
  REQUIRE(t.snapshot(25000)["state"] == "returning");
  REQUIRE(t.snapshot(25000)["songTick"].get<double>() < 960);
  t.tick(120256);
  REQUIRE(t.snapshot(120256)["songTick"] == 480);
}
TEST_CASE("Wrong attack and incomplete deadline count one missed group; pause "
          "retry requires fresh chord",
          "[transport][scoring]") {
  Engine e;
  Transport t(e);
  t.load(model());
  t.command("transport.play", {}, 0);
  t.input({EventType::On, Bus::Live, 1, 0, 65, 90, 1, 24256});
  t.tick(33000);
  REQUIRE(t.snapshot(33000)["misses"] == 1);
  t.load(model());
  t.command("transport.configure", {{"mode", "pauseOnMiss"}}, 0);
  t.command("transport.play", {}, 0);
  t.tick(33000);
  REQUIRE(t.snapshot(33000)["retry"] == true);
  REQUIRE(t.snapshot(33000)["songTick"] == 480);
  t.input({EventType::On, Bus::Live, 1, 0, 60, 90, 2, 33001});
  REQUIRE(t.snapshot(33001)["retry"] == false);
  REQUIRE(t.snapshot(33001)["hits"] == 1);
  REQUIRE(t.snapshot(33001)["state"] == "playing");
}
TEST_CASE("Physical chord assembly is inclusive150ms independently of wider "
          "timing tolerance",
          "[transport][scoring]") {
  for (unsigned delta : {7200, 7201}) {
    Engine e;
    Transport t(e);
    auto s = model();
    auto n = s["notes"][0];
    n["id"] = 3;
    n["midiNote"] = 64;
    s["notes"].push_back(n);
    t.load(s);
    t.command("transport.configure", {{"toleranceMs", 1000}}, 0);
    t.command("transport.play", {}, 0);
    t.input({EventType::On, Bus::Live, 1, 0, 60, 90, 1, 24256});
    t.input({EventType::On, Bus::Live, 1, 0, 64, 90, 2, 24256 + delta});
    REQUIRE(t.snapshot(24256 + delta)["hits"] == (delta == 7200 ? 1 : 0));
    REQUIRE(t.snapshot(24256 + delta)["misses"] == (delta == 7200 ? 0 : 1));
  }
}
TEST_CASE("Play Along tempo edits update global clicks without toggling them", "[tempo-sync]") {
  for (bool running : {false, true}) {
    Engine e({"missing.sfz", 48000, 128, false, true});
    Transport t(e);
    t.load(model());
    if (running)
      REQUIRE(e.command({{"version", 1}, {"id", 1}, {"type", "metronome.start"}, {"params", json::object()}})["ok"] == true);
    std::vector<float> out(96000 * 2);
    e.render(out.data(), 128);
    t.command("transport.tempo", {{"bpm", 90}}, e.sampleFrame());
    e.render(out.data(), 96000);
    REQUIRE(e.snapshot()["metronome"]["tempo"] == 90);
    if (running) REQUIRE(e.snapshot()["metronome"]["activeTempo"] == 90);
    REQUIRE(e.snapshot()["metronome"]["running"] == running);
    t.command("transport.configure", {{"speed", .5}}, e.sampleFrame());
    e.render(out.data(), 96000);
    REQUIRE(e.snapshot()["metronome"]["tempo"] == 60);
    if (running) REQUIRE(e.snapshot()["metronome"]["activeTempo"] == 60);
    REQUIRE(e.snapshot()["metronome"]["running"] == running);
  }
}
TEST_CASE("Native tempo edits preserve position and unsupported Follow maps "
          "have a truthful diagnostic",
          "[transport]") {
  Engine e;
  Transport t(e);
  auto s = model();
  t.load(s);
  t.command("transport.play", {}, 0);
  t.command("transport.tempo", {{"bpm", 90}}, 12256);
  REQUIRE(t.snapshot(12256)["speed"] == .75);
  REQUIRE(t.snapshot(12256)["songTick"] == 240);
  REQUIRE(t.snapshot(12256)["tempo"] == 90);
  s["tempoMap"][0]["microseconds"] = 1;
  prepareSongTiming(s);
  s["durationSeconds"] = tickSeconds(s, 1920);
  t.load(s);
  t.command("transport.play", {}, 0);
  REQUIRE_FALSE(t.snapshot(0)["followError"].get<std::string>().empty());
}
TEST_CASE("Pathological accompaniment density falls back without stopping "
          "original live or clicks",
          "[transport]") {
  Engine e;
  Transport t(e);
  auto s = model();
  s["ppq"] = 1;
  s["tempoMap"][0]["microseconds"] = 1;
  prepareSongTiming(s);
  s["durationSeconds"] = tickSeconds(s, 1920);
  t.load(s);
  t.command("transport.configure", {{"accompaniment", "Triad"}}, 0);
  t.command("transport.play", {}, 0);
  t.tick(0);
  REQUIRE_FALSE(t.snapshot(0)["accompanimentError"].get<std::string>().empty());
  REQUIRE(t.snapshot(0)["state"] == "playing");
}
TEST_CASE("Named input exhaustion never allocates reserved native playback or "
          "example sources",
          "[transport][sources]") {
  EngineOptions options;
  options.pianoPath = std::string(SR_ROOT) + "/native/tests/fixture.sfz";
  options.preloadSynchronously = true;
  Engine e(options);
  std::vector<float> audio(256);
  for (unsigned i = 1; i < FirstReservedSource; ++i) {
    auto reply = e.command({{"version", 1},
                            {"id", i},
                            {"type", "virtual.noteOn"},
                            {"params",
                             {{"source", "allocation-" + std::to_string(i)},
                              {"channel", 0},
                              {"note", 60},
                              {"velocity", 90},
                              {"instance", i}}}});
    REQUIRE(reply["ok"] == true);
    e.render(audio.data(), 128);
  }
  auto last = e.command({{"version", 1},
                         {"id", "overflow"},
                         {"type", "virtual.noteOn"},
                         {"params",
                          {{"source", "overflow"},
                           {"channel", 0},
                           {"note", 60},
                           {"velocity", 90},
                           {"instance", 999}}}});
  REQUIRE(last["ok"] == false);
  for (auto &n : e.snapshot()["heldNotes"])
    REQUIRE(n["sourceId"].get<unsigned>() < FirstReservedSource);
}
TEST_CASE("CC120 releases existing channel tails while another channel remains "
          "audible",
          "[transport][controllers]") {
  EngineOptions options;
  options.pianoPath = std::string(SR_ROOT) + "/native/tests/fixture.sfz";
  options.preloadSynchronously = true;
  Engine e(options);
  std::vector<float> audio(9600);
  e.submit({EventType::Controller, Bus::Playback, SongSource, 3, 10, 0});
  e.submit({EventType::Controller, Bus::Playback, SongSource, 4, 10, 127});
  e.submit({EventType::On, Bus::Playback, SongSource, 3, 60, 100, 1});
  e.submit({EventType::On, Bus::Playback, SongSource, 4, 64, 100, 2});
  e.render(audio.data(), 4800);
  e.submit({EventType::Off, Bus::Playback, SongSource, 3, 60, 0, 1});
  e.render(audio.data(), 128);
  e.submit({EventType::Controller, Bus::Playback, SongSource, 3, 120, 0});
  e.render(audio.data(), 4800);
  double left = 0, right = 0;
  for (unsigned i = 3600; i < 4800; ++i) {
    left += std::abs(audio[i * 2]);
    right += std::abs(audio[i * 2 + 1]);
  }
  REQUIRE(left < .00001);
  REQUIRE(right > .001);
  REQUIRE(e.snapshot()["heldNotes"].size() == 1);
  REQUIRE(e.snapshot()["heldNotes"][0]["channel"] == 4);
}
TEST_CASE("Restore rejects dense owners and queue pressure truthfully",
          "[review-restore]") {
  Engine e;
  Transport t(e);
  auto s = model();
  auto n = s["notes"][1];
  n["startTick"] = 0;
  n["endTick"] = 1920;
  s["notes"] = json::array();
  for (unsigned i = 0; i < 1100; ++i) {
    n["id"] = i + 1;
    s["notes"].push_back(n);
  }
  t.load(s);
  t.command("transport.seek", {{"tick", 480}}, 0);
  REQUIRE_THROWS(t.command("transport.play", {}, 0));
  REQUIRE(t.snapshot(0)["state"] != "playing");
  REQUIRE_FALSE(t.snapshot(0)["error"].get<std::string>().empty());
  t.load(model());
  t.command("transport.seek", {{"tick", 480}}, 0);
  e.submit({EventType::On, Bus::Live, 1, 0, 76, 100, 99});
  e.submit({EventType::On, Bus::Playback, AccompanimentSource, 0, 60, 100, 100});
  std::vector<float> audio(256); e.render(audio.data(),128);
  unsigned accepted = 0;
  while (e.submit(
      {EventType::Controller, Bus::Playback, SongSource, 0, 7, 100, 0, 999999}))
    ++accepted;
  REQUIRE(accepted > 0);
  REQUIRE_THROWS(t.command("transport.play", {}, 0));
  REQUIRE(t.snapshot(0)["state"] != "playing");
  e.render(audio.data(),128);
  auto held=e.snapshot()["heldNotes"];
  REQUIRE(held.size()==2);
  REQUIRE(std::any_of(held.begin(),held.end(),[](const auto &n){return n["sourceId"]==AccompanimentSource;}));
  REQUIRE(std::any_of(held.begin(),held.end(),[](const auto &n){return n["bus"]=="live";}));
}
TEST_CASE("Restart and seek select fresh scoring groups", "[review-score]") {
  Engine e;
  Transport t(e);
  t.load(model());
  t.command("transport.play", {}, 0);
  t.input({EventType::On, Bus::Live, 1, 0, 65, 90, 1, 24256});
  REQUIRE(t.snapshot(24256)["misses"] == 1);
  t.command("transport.restart", {}, 0);
  t.input({EventType::On, Bus::Live, 1, 0, 65, 90, 2, 24256});
  REQUIRE(t.snapshot(24256)["misses"] == 1);
  t.command("transport.seek", {{"tick", 480}}, 0);
  t.input({EventType::On, Bus::Live, 1, 0, 65, 90, 3, 256});
  REQUIRE(t.snapshot(256)["misses"] == 2);
}
TEST_CASE("Playback controller owners isolate identical accompaniment channel",
          "[review-source]") {
  EngineOptions o;
  o.pianoPath = std::string(SR_ROOT) + "/native/tests/fixture.sfz";
  o.preloadSynchronously = true;
  Engine e(o);
  std::vector<float> a(9600);
  e.submit({EventType::On, Bus::Playback, SongSource, 0, 60, 100, 1});
  e.submit({EventType::On, Bus::Playback, AccompanimentSource, 0, 60, 100, 2});
  e.render(a.data(), 128);
  e.submit({EventType::Controller, Bus::Playback, SongSource, 0, 123, 0});
  e.render(a.data(), 128);
  REQUIRE(e.snapshot()["heldNotes"].size() == 1);
  REQUIRE(e.snapshot()["heldNotes"][0]["sourceId"] == AccompanimentSource);
  REQUIRE_FALSE(e.submit({EventType::On, Bus::Playback, 1, 0, 60, 100, 3}));
}
TEST_CASE("Dense tempo metadata consumes bounded pump budget",
          "[review-metadata]") {
  Engine e;
  Transport t(e);
  auto s = model();
  s["tempoMap"] = json::array();
  for (unsigned i = 0; i < 5000; ++i)
    s["tempoMap"].push_back({{"tick", i * .001}, {"microseconds", 500000}});
  t.load(s);
  t.command("transport.play", {}, 0);
  t.tick(0);
  REQUIRE(t.snapshot(0)["pumpEntries"] == 2048);
}
TEST_CASE("Late sparse seek ignores more than65536 prior notes",
          "[review-restore]") {
  Engine e;
  Transport t(e);
  auto s = model();
  auto n = s["notes"][1];
  s["notes"] = json::array();
  for (unsigned i = 0; i < 66000; ++i) {
    n["id"] = i + 1;
    n["startTick"] = i * 2;
    n["endTick"] = i * 2 + 1;
    s["notes"].push_back(n);
  }
  s["durationTicks"] = 140000;
  s["durationSeconds"] = 140000. / 960;
  t.load(s);
  t.command("transport.seek", {{"tick", 132001}}, 0);
  REQUIRE_NOTHROW(t.command("transport.play", {}, 0));
  REQUIRE(t.snapshot(0)["state"] == "playing");
}
TEST_CASE("Frozen retry retains its one miss while pause resume and loop "
          "select fresh groups",
          "[review-score]") {
  Engine e;
  Transport t(e);
  t.load(model());
  t.command("transport.configure", {{"mode", "pauseOnMiss"}}, 0);
  t.command("transport.play", {}, 0);
  t.input({EventType::On, Bus::Live, 1, 0, 65, 90, 1, 24256});
  t.command("transport.play", {}, 25000);
  t.input({EventType::On, Bus::Live, 1, 0, 65, 90, 2, 25256});
  REQUIRE(t.snapshot(25256)["misses"] == 1);
  t.load(model());
  t.command("transport.configure",
            {{"mode", "continuous"}, {"loop", true}, {"a", 480}, {"b", 960}},
            0);
  t.command("transport.seek", {{"tick", 480}}, 0);
  t.command("transport.play", {}, 0);
  t.input({EventType::On, Bus::Live, 1, 0, 65, 90, 3, 256});
  t.tick(25000);
  t.input({EventType::On, Bus::Live, 1, 0, 65, 90, 4, 25001});
  REQUIRE(t.snapshot(25001)["misses"] == 1);
  t.tick(120256);
  t.input({EventType::On, Bus::Live, 1, 0, 65, 90, 5, 120513});
  REQUIRE(t.snapshot(120513)["misses"] == 2);
}
TEST_CASE(
    "Delayed source stereo curves affect held and release layers independently",
    "[review-source]") {
  EngineOptions o;
  o.pianoPath = std::string(SR_ROOT) + "/native/tests/fixture.sfz";
  o.preloadSynchronously = true;
  Engine e(o);
  std::vector<float> a(9600);
  e.submit({EventType::Controller, Bus::Playback, SongSource, 0, 10, 0});
  e.submit(
      {EventType::Controller, Bus::Playback, AccompanimentSource, 0, 10, 127});
  e.submit({EventType::On, Bus::Playback, SongSource, 0, 60, 100, 1});
  e.submit({EventType::On, Bus::Playback, AccompanimentSource, 0, 60, 100, 2});
  e.render(a.data(), 4800);
  e.submit({EventType::Controller, Bus::Playback, SongSource, 0, 7, 0, 0,
            e.sampleFrame() + 64});
  e.render(a.data(), 128);
  double left = 0, right = 0;
  for (unsigned i = 64; i < 128; ++i) {
    left += std::abs(a[i * 2]);
    right += std::abs(a[i * 2 + 1]);
  }
  REQUIRE(left < .00001);
  REQUIRE(right > .001);
  e.submit({EventType::Off, Bus::Playback, AccompanimentSource, 0, 60, 0, 2});
  e.render(a.data(), 128);
  e.submit({EventType::Controller, Bus::Playback, SongSource, 0, 120, 0});
  e.render(a.data(), 128);
  right = 0;
  for (unsigned i = 0; i < 128; ++i)
    right += std::abs(a[i * 2 + 1]);
  REQUIRE(right > .001);
  e.submit({EventType::Disconnect, Bus::Playback, SongSource});
  e.render(a.data(), 128);
  right = 0;
  for (unsigned i = 0; i < 128; ++i)
    right += std::abs(a[i * 2 + 1]);
  REQUIRE(right > .001);
  e.submit({EventType::Disconnect, Bus::Playback, AccompanimentSource});
  e.render(a.data(), 4800);
  right = 0;
  for (unsigned i = 3600; i < 4800; ++i)
    right += std::abs(a[i * 2 + 1]);
  REQUIRE(right < .00001);
}
TEST_CASE(
    "Loop restoration overload stops truthfully without touching live owner",
    "[review-restore]") {
  Engine e;
  Transport t(e);
  auto s = model();
  auto n = s["notes"][1];
  s["notes"] = json::array();
  for (unsigned i = 0; i < 1100; ++i) {
    n["id"] = i + 1;
    n["startTick"] = 0;
    n["endTick"] = 1920;
    s["notes"].push_back(n);
  }
  t.load(s);
  t.command("transport.configure", {{"loop", true}, {"a", 480}, {"b", 960}}, 0);
  REQUIRE(e.submit({EventType::On, Bus::Live, 1, 0, 106, 90, 777}));
  std::vector<float> audio(256);
  e.render(audio.data(), 128);
  // Playback now enters directly at A, so restoration rejects on Play itself.
  REQUIRE_THROWS(t.command("transport.play", {}, 0));
  t.tick(47000);
  REQUIRE(t.snapshot(47000)["state"] != "playing");
  REQUIRE_FALSE(t.snapshot(47000)["error"].get<std::string>().empty());
  e.render(audio.data(), 128);
  REQUIRE(e.snapshot()["heldNotes"].size() == 1);
  REQUIRE(e.snapshot()["heldNotes"][0]["bus"] == "live");
}
TEST_CASE("Source recovery cancels newly spawned release-trigger layers", "[review-source]") {
 EngineOptions o;o.pianoPath=std::string(SR_ROOT)+"/native/tests/fixture-release.sfz";o.preloadSynchronously=true;Engine e(o);std::vector<float>a(9600);
 e.submit({EventType::Controller,Bus::Playback,SongSource,0,10,0});
 e.submit({EventType::Controller,Bus::Playback,AccompanimentSource,0,10,127});
 e.submit({EventType::On,Bus::Playback,SongSource,0,60,100,1});
 e.submit({EventType::On,Bus::Playback,AccompanimentSource,0,60,100,2});e.render(a.data(),4800);
 e.submit({EventType::Disconnect,Bus::Playback,SongSource});e.render(a.data(),4800);
 double left=0,right=0;for(unsigned i=3600;i<4800;++i){left+=std::abs(a[i*2]);right+=std::abs(a[i*2+1]);}
 REQUIRE(left<.00001);REQUIRE(right>.001);
}
TEST_CASE("Restart from first frozen retry starts a fresh missed group", "[review-score]") {
  Engine e; Transport t(e); t.load(model());
  t.command("transport.configure", {{"mode", "pauseOnMiss"}}, 0);
  t.command("transport.play", {}, 0);
  t.input({EventType::On, Bus::Live, 1, 0, 65, 90, 1, 24256});
  REQUIRE(t.snapshot(24256)["retry"] == true);
  REQUIRE(t.snapshot(24256)["misses"] == 1);
  t.command("transport.restart", {}, 25000);
  REQUIRE(t.snapshot(25000)["misses"] == 0);
  REQUIRE(t.snapshot(25000)["retry"] == false);
  t.input({EventType::On, Bus::Live, 1, 0, 65, 90, 2, 49256});
  REQUIRE(t.snapshot(49256)["misses"] == 1);
  REQUIRE(t.snapshot(49256)["retry"] == true);
}
