#include <catch2/catch_test_macros.hpp>
#include <sightreading/Transport.hpp>
using namespace sightreading;
using json = nlohmann::json;
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
  REQUIRE(t.snapshot(25000)["songTick"].get<double>() < 500);
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
