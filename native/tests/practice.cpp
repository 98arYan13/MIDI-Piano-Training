#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <sightreading/Engine.hpp>
#include <sightreading/Practice.hpp>
using namespace sightreading;
TEST_CASE("Native theory spells all keys and minor alterations", "[practice]") {
  for (auto key : {"C", "G", "D", "A", "E", "B", "F", "Bb", "Eb", "Ab", "Db",
                   "Gb", "Chromatic"}) {
    auto notes = scalePitches(key, "major", 21, 108);
    REQUIRE(notes.size() >= 49);
    for (auto n : notes)
      REQUIRE(n.midi >= 21);
  }
  auto gb = scalePitches("Gb", "major", 59, 59);
  REQUIRE(gb.size() == 1);
  REQUIRE(gb[0].letter == 'c');
  REQUIRE(gb[0].accidental == -1);
  REQUIRE(gb[0].octave == 4);
  auto harmonic = scalePitches("A", "harmonicMinor", 68, 68);
  REQUIRE(harmonic.size() == 1);
  REQUIRE(harmonic[0].letter == 'g');
  REQUIRE(harmonic[0].accidental == 1);
}
TEST_CASE("Seeded native Staff generators honor range hands and options",
          "[practice]") {
  auto c = defaultPracticeConfig();
  for (auto mode : {"random", "triads", "sevenths", "progression", "position",
                    "intervals", "chordSymbol"}) {
    c["generator"] = mode;
    auto a = generatePractice(c, 71, 24), b = generatePractice(c, 71, 24);
    REQUIRE(a.size() == 24);
    for (unsigned i = 0; i < a.size(); ++i) {
      REQUIRE(a[i].jsonValue() == b[i].jsonValue());
      for (auto p : a[i].pitches) {
        REQUIRE(p.midi >= 48);
        REQUIRE(p.midi <= 84);
      }
    }
  }
  c["generator"] = "intervals";
  c["generatorOptions"]["intervals"] = json::array();
  REQUIRE_THROWS_WITH(generatePractice(c, 1),
                      "Select at least one interval (2nd through 7th).");
  c = defaultPracticeConfig();
  c["generator"] = "triads";
  c["range"] = {{"min", 60}, {"max", 61}};
  REQUIRE_THROWS(generatePractice(c, 1));
  c = defaultPracticeConfig();
  c["generator"] = "position";
  auto positions = generatePractice(c, 1, 10);
  for (auto &p : positions)
    REQUIRE(p.fingering >= 1);
}
static InputEvent on(int pitch, uint64_t frame, uint64_t instance = 1,
                     unsigned source = 126) {
  InputEvent e;
  e.note = pitch;
  e.velocity = 100;
  e.frame = frame;
  e.instance = instance;
  e.source = source;
  return e;
}
TEST_CASE("Wait scores physical chords within 150ms and permits retry",
          "[practice]") {
  Practice p;
  p.configure({{"range", {{"min", 60}, {"max", 60}}}, {"smoothness", 0}});
  p.start(100);
  p.input(on(61, 200));
  REQUIRE(p.snapshot(200)["misses"] == 1);
  auto wrongOff = on(61, 300);
  wrongOff.type = EventType::Off;
  p.input(wrongOff);
  p.input(on(60, 400, 2));
  REQUIRE(p.snapshot(400)["hits"] == 1);
  p.reset(500);
  REQUIRE(p.snapshot(500)["hits"] == 0);
  REQUIRE(p.snapshot(500)["pressed"].size() == 1);
  InputEvent disconnect;
  disconnect.type = EventType::Disconnect;
  disconnect.source = 126;
  disconnect.frame = 600;
  p.input(disconnect);
  REQUIRE(p.snapshot(600)["pressed"].empty());
}
TEST_CASE(
    "Scroll misses are exact native frame deadlines and bus inputs ignored",
    "[practice]") {
  Practice p;
  p.configure({{"mode", "scroll"},
               {"range", {{"min", 60}, {"max", 60}}},
               {"tempo", 120},
               {"smoothness", 0}});
  p.start(1000);
  auto e = on(60, 1000);
  e.bus = Bus::Example;
  p.input(e);
  REQUIRE(p.snapshot(1000)["hits"] == 0);
  p.tick(1000 + 7201);
  REQUIRE(p.snapshot(8201)["misses"] == 1);
  p.input(on(60, 25000));
  REQUIRE(p.snapshot(25000)["hits"] == 1);
}
TEST_CASE(
    "Native applied disconnect and bus cancellation reach input consumers",
    "[practice]") {
  std::vector<InputEvent> events;
  Engine e;
  e.setInputHook(
      [](const InputEvent &v, uint64_t f, void *c) noexcept {
        auto x = v;
        x.frame = f;
        static_cast<std::vector<InputEvent> *>(c)->push_back(x);
      },
      &events);
  events.reserve(16);
  float audio[256]{};
  e.submit(on(60, 0));
  e.render(audio, 128);
  InputEvent d;
  d.type = EventType::Disconnect;
  d.source = 126;
  e.submit(d);
  e.render(audio, 128);
  REQUIRE(events.back().type == EventType::Disconnect);
  d.type = EventType::StopBus;
  d.bus = Bus::Playback;
  e.submit(d);
  e.render(audio, 128);
  REQUIRE(events.back().type == EventType::StopBus);
}

TEST_CASE("Physical exact pitch set, owner isolation and reset freshness",
          "[practice]") {
  Practice p;
  p.configure({{"range", {{"min", 60}, {"max", 60}}}, {"smoothness", 0}});
  p.start(100);
  p.input(on(61, 200, 1, 125));
  p.input(on(60, 300, 2, 126));
  REQUIRE(p.snapshot(300)["hits"] == 0);
  InputEvent off = on(61, 400, 1, 125);
  off.type = EventType::Off;
  p.input(off);
  p.input(on(60, 500, 3, 127));
  REQUIRE(p.snapshot(500)["hits"] == 1);
  p.reset(600);
  REQUIRE(p.snapshot(600)["hits"] == 0);
  REQUIRE(p.snapshot(600)["pressed"].size() == 2);
  InputEvent pedal;
  pedal.type = EventType::Pedal;
  pedal.velocity = 127;
  pedal.frame = 650;
  p.input(pedal);
  REQUIRE(p.snapshot(650)["hits"] == 0);
  InputEvent d;
  d.type = EventType::Disconnect;
  d.source = 126;
  d.frame = 700;
  p.input(d);
  REQUIRE(p.snapshot(700)["pressed"].size() == 1);
  REQUIRE(p.snapshot(700)["pressed"][0]["sourceId"] == 127);
  off = on(60, 800, 3, 127);
  off.type = EventType::Off;
  p.input(off);
  p.input(on(60, 900, 4, 127));
  REQUIRE(p.snapshot(900)["hits"] == 1);
}
TEST_CASE("Wait incomplete chord expires at native 150ms boundary",
          "[practice]") {
  Practice p;
  p.configure({{"generator", "triads"},
               {"range", {{"min", 60}, {"max", 67}}},
               {"smoothness", 0},
               {"generatorOptions", {{"inversions", false}}}});
  p.start(100);
  p.input(on(60, 200, 1));
  p.input(on(64, 7000, 2));
  p.tick(7400);
  REQUIRE(p.snapshot(7400)["misses"] == 0);
  p.tick(7401);
  REQUIRE(p.snapshot(7401)["misses"] == 1);
  REQUIRE(p.snapshot(7401)["hits"] == 0);
  p.input(on(60, 8000, 3));
  p.input(on(64, 8100, 4));
  p.input(on(67, 8200, 5));
  REQUIRE(p.snapshot(8200)["hits"] == 1);
}
TEST_CASE("Generator options produce real chord classes inversions spans and "
          "smoothness",
          "[practice]") {
  auto c = defaultPracticeConfig();
  c["range"] = {{"min", 36}, {"max", 96}};
  for (auto key :
       {"C", "G", "D", "A", "E", "B", "F", "Bb", "Eb", "Ab", "Db", "Gb"})
    for (auto scale :
         {"major", "naturalMinor", "harmonicMinor", "melodicMinor"}) {
      c["key"] = key;
      c["generatorOptions"]["scale"] = scale;
      c["generator"] = "triads";
      auto ts = generatePractice(c, 17, 8);
      for (auto t : ts)
        REQUIRE(t.pitches.size() == 3);
    }
  c = defaultPracticeConfig();
  c["range"] = {{"min", 36}, {"max", 96}};
  c["generator"] = "random";
  c["generatorOptions"]["hands"] = 1;
  c["generatorOptions"]["notes"] = 5;
  c["generatorOptions"]["chordBased"] = true;
  auto random = generatePractice(c, 10, 16);
  for (auto t : random) {
    REQUIRE(t.pitches.size() == 5);
    REQUIRE(t.pitches.back().midi - t.pitches.front().midi <= 12);
  }
  c = defaultPracticeConfig();
  c["generator"] = "sevenths";
  c["generatorOptions"]["openVoicing"] = true;
  auto open = generatePractice(c, 9, 16);
  for (auto t : open) {
    REQUIRE(t.pitches.size() == 4);
    REQUIRE(t.pitches.back().midi - t.pitches.front().midi >= 12);
  }
  for (int smooth = 0; smooth <= 6; ++smooth) {
    c = defaultPracticeConfig();
    c["smoothness"] = smooth;
    auto ts = generatePractice(c, 27, 32);
    for (unsigned i = 1; i < ts.size(); ++i)
      if (smooth)
        REQUIRE(std::abs(ts[i].pitches[0].midi - ts[i - 1].pitches[0].midi) <=
                14 - 2 * smooth);
  }
  for (auto progression :
       {"autumnLeaves", "fifties", "circle", "substitution"}) {
    c = defaultPracticeConfig();
    c["generator"] = "progression";
    c["generatorOptions"]["progression"] = progression;
    REQUIRE(generatePractice(c, 41, 16).size() == 16);
  }
  c = defaultPracticeConfig();
  c["generator"] = "intervals";
  c["generatorOptions"]["intervals"] = json::array({7});
  auto intervals = generatePractice(c, 91, 8);
  for (auto t : intervals) {
    const std::string letters = "cdefgab";
    int a = int(letters.find(t.pitches[0].letter)) + 7 * t.pitches[0].octave,
        b = int(letters.find(t.pitches[1].letter)) + 7 * t.pitches[1].octave;
    REQUIRE(b - a == 6);
  }
}
TEST_CASE("Chord common classes and ignore-above rejection are bounded",
          "[practice]") {
  auto c = defaultPracticeConfig();
  c["staff"] = "chordSymbol";
  c["generatorOptions"]["tones"] = 4;
  c["generatorOptions"]["multiKey"] = true;
  c["generatorOptions"]["keys"] = json::array({"C", "F", "G"});
  c["generatorOptions"]["commonNotes"] = 2;
  auto ts = generatePractice(c, 33, 24);
  for (unsigned i = 1; i < ts.size(); ++i) {
    std::set<int> a, b;
    for (auto p : ts[i].pitches)
      a.insert(p.midi % 12);
    for (auto p : ts[i - 1].pitches)
      b.insert(p.midi % 12);
    int common = 0;
    for (int n : a)
      common += b.count(n);
    REQUIRE(common >= 2);
  }
  c["generatorOptions"]["ignoreAbove"] = 21;
  REQUIRE_THROWS(generatePractice(c, 33, 2));
  c["generatorOptions"]["keys"] = json::array();
  REQUIRE_THROWS(generatePractice(c, 33));
}

TEST_CASE("Basic substitution uses flat-II in place of dominant",
          "[practice]") {
  auto c = defaultPracticeConfig();
  c["generator"] = "progression";
  c["generatorOptions"]["progression"] = "substitution";
  c["smoothness"] = 0;
  c["generatorOptions"]["inversions"] = false;
  auto ts = generatePractice(c, 88, 4);
  REQUIRE(ts[3].pitches.front().midi % 12 == 1);
  REQUIRE(ts[3].symbol == "Db");
}
TEST_CASE("Chromatic chord generation still makes triads rather than clusters",
          "[practice]") {
  auto c = defaultPracticeConfig();
  c["key"] = "Chromatic";
  c["generator"] = "triads";
  c["generatorOptions"]["inversions"] = false;
  auto ts = generatePractice(c, 4, 8);
  for (auto t : ts) {
    int third = t.pitches[1].midi - t.pitches[0].midi,
        fifth = t.pitches[2].midi - t.pitches[0].midi;
    REQUIRE((third == 3 || third == 4));
    REQUIRE((fifth == 6 || fifth == 7));
  }
}
TEST_CASE("Scroll timeline does not accumulate integer quarter-frame rounding",
          "[practice]") {
  Practice p;
  p.configure({{"range", {{"min", 60}, {"max", 60}}},
               {"smoothness", 0},
               {"mode", "scroll"},
               {"tempo", 137}});
  p.start(100);
  auto s = p.snapshot(100);
  REQUIRE(s["notes"][63]["startFrame"] == uint64_t(63) * 48000 * 60 / 137);
  REQUIRE(s["notes"][63]["endFrame"] == uint64_t(64) * 48000 * 60 / 137);
}
TEST_CASE(
    "Event gap stops evaluation and reconciles only physically held live notes",
    "[practice]") {
  Practice p;
  p.configure({{"range", {{"min", 60}, {"max", 60}}}, {"smoothness", 0}});
  p.start(100);
  p.input(on(60, 200, 1));
  p.recoverHeld(json::array({{{"sourceId", 127},
                              {"channel", 0},
                              {"instance", 2},
                              {"note", 64},
                              {"bus", "live"},
                              {"held", true}},
                             {{"sourceId", 126},
                              {"channel", 0},
                              {"instance", 3},
                              {"note", 60},
                              {"bus", "live"},
                              {"held", false}},
                             {{"sourceId", 126},
                              {"channel", 0},
                              {"instance", 4},
                              {"note", 60},
                              {"bus", "example"},
                              {"held", true}}}),
                300);
  auto s = p.snapshot(300);
  REQUIRE(s["running"] == false);
  REQUIRE(s["pressed"].size() == 1);
  REQUIRE(s["pressed"][0]["note"] == 64);
  REQUIRE(!s["error"].get<std::string>().empty());
  p.start(400);
  REQUIRE(p.snapshot(400)["error"] == "");
}
TEST_CASE("Position drills use a fixed five-finger hand and starting finger "
          "chooses first pitch",
          "[practice]") {
  auto c = defaultPracticeConfig();
  c["generator"] = "position";
  c["range"] = {{"min", 60}, {"max", 67}};
  c["generatorOptions"]["startingFinger"] = 3;
  auto ts = generatePractice(c, 79, 24);
  REQUIRE(ts[0].pitches.size() == 1);
  REQUIRE(ts[0].pitches[0].midi == 64);
  REQUIRE(ts[0].fingering == 3);
  const int pitches[]{60, 62, 64, 65, 67};
  for (auto t : ts) {
    REQUIRE(t.pitches.size() == 1);
    REQUIRE(t.pitches[0].midi == pitches[t.fingering - 1]);
  }
  c["generatorOptions"]["startingFinger"] = 5;
  REQUIRE(generatePractice(c, 79, 1)[0].pitches[0].midi == 67);
  c["range"]["max"] = 65;
  REQUIRE_THROWS(generatePractice(c, 79));
}
TEST_CASE("Scroll incomplete chord at its deadline counts one missed target",
          "[practice]") {
  Practice p;
  p.configure({{"generator", "triads"},
               {"range", {{"min", 60}, {"max", 67}}},
               {"smoothness", 0},
               {"mode", "scroll"},
               {"generatorOptions", {{"inversions", false}}}});
  p.start(100);
  p.input(on(60, 100, 1));
  p.tick(7301);
  REQUIRE(p.snapshot(7301)["misses"] == 1);
  REQUIRE(p.snapshot(7301)["index"] == 1);
}
TEST_CASE(
    "Chromatic progression chooses one seeded tonic for the whole progression",
    "[practice]") {
  auto c = defaultPracticeConfig();
  c["key"] = "Chromatic";
  c["generator"] = "progression";
  c["generatorOptions"]["progression"] = "fifties";
  c["generatorOptions"]["inversions"] = false;
  c["smoothness"] = 0;
  auto ts = generatePractice(c, 17, 8);
  const int degrees[]{0, 9, 5, 7};
  int tonic = ts[0].pitches[0].midi % 12;
  for (unsigned i = 0; i < ts.size(); ++i)
    REQUIRE(ts[i].pitches[0].midi % 12 == (tonic + degrees[i % 4]) % 12);
}
