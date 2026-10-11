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
  REQUIRE(t.snapshot(25001)["misses"] == 2);
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
  t.command("transport.play", {}, 0);
  t.tick(47000);
  REQUIRE(t.snapshot(47000)["state"] != "playing");
  REQUIRE_FALSE(t.snapshot(47000)["error"].get<std::string>().empty());
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
