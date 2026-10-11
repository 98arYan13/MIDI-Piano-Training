#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <sightreading/Song.hpp>
using namespace sightreading;
static std::string smf(std::string track, unsigned format = 0,
                       unsigned division = 480) {
  std::string b = "MThd";
  b.append(
      {0, 0, 0, 6, 0, char(format), 0, 1, char(division >> 8), char(division)});
  b += "MTrk";
  auto n = track.size();
  b.append({char(n >> 24), char(n >> 16), char(n >> 8), char(n)});
  return b + track;
}
static std::string bytes(std::initializer_list<unsigned> v) {
  std::string b;
  for (auto x : v)
    b += char(x);
  return b;
}
TEST_CASE("SMF binary preserves overlap controllers and native tempo timing",
          "[song]") {
  auto b =
      smf(bytes({0,    0x90, 60,   90,   0,    0x90, 60, 80,   0,    0xb0, 64,
                 127,  0x83, 0x60, 0x80, 60,   0,    0,  0xff, 0x51, 3,    0x0f,
                 0x42, 0x40, 0x83, 0x60, 0x90, 60,   0,  0,    0xff, 0x2f, 0}));
  auto s = importMidi(b, "Overlap");
  REQUIRE(s["notes"].size() == 2);
  REQUIRE(s["notes"][0]["endTick"] == 480);
  REQUIRE(s["notes"][1]["endTick"] == 960);
  REQUIRE(s["controllers"][0]["controller"] == 64);
  REQUIRE(s["meterMap"][0]["numerator"] == 4);
  REQUIRE(s["tempoMap"].size() == 2);
  REQUIRE(tickSeconds(s, 960) == Catch::Approx(1.5));
  REQUIRE(secondsTick(s, 1.5) == Catch::Approx(960));
}
TEST_CASE("SMF rejects malformed empty unsupported and excessive inputs",
          "[song]") {
  for (auto b : {std::string{}, smf(bytes({0, 0xff, 0x2f, 0})),
                 smf(bytes({0, 0x90, 60})),
                 smf(bytes({0, 0x90, 60, 80, 0, 0xff, 0x2f, 0}), 2),
                 smf(bytes({0, 0xff, 0x2f, 0}), 0, 0xe728)})
    REQUIRE_THROWS_AS(importMidi(b, "Invalid"), std::invalid_argument);
  REQUIRE_THROWS_AS(importMidi(std::string(20 * 1024 * 1024 + 1, 'a'), "Large"),
                    std::invalid_argument);
}
static std::string vlq(unsigned value) {
  std::string v(1, char(value & 127));
  while ((value >>= 7))
    v.insert(v.begin(), char((value & 127) | 128));
  return v;
}
TEST_CASE("SMF type1 keeps meter key raw controller order and independent "
          "overlapping tracks",
          "[song]") {
  auto first = smf(bytes({0,    0xff, 0x58, 4,    6, 3, 24,   8,    0,
                          0xff, 0x59, 2,    0xfe, 1, 0, 0x90, 60,   91,
                          0x83, 0x60, 0x80, 60,   0, 0, 0xff, 0x2f, 0}),
                   1);
  auto second =
      smf(bytes({0,  0xb1, 7,    90,   0,  0xb1, 11, 50,   0,    0x91, 60,
                 82, 0x87, 0x40, 0x81, 60, 0,    0,  0xff, 0x2f, 0}));
  first[11] = 2;
  first += second.substr(14);
  auto s = importMidi(first, "Two tracks");
  REQUIRE(s["tracks"].size() == 2);
  REQUIRE(s["notes"].size() == 2);
  REQUIRE(s["notes"][0]["endTick"] == 480);
  REQUIRE(s["notes"][1]["endTick"] == 960);
  REQUIRE(s["meterMap"][0]["numerator"] == 6);
  REQUIRE(s["meterMap"][0]["denominator"] == 8);
  REQUIRE(s["keyMap"][0]["sharps"] == -2);
  REQUIRE(s["keyMap"][0]["minor"] == true);
  REQUIRE(s["controllers"][0]["controller"] == 7);
  REQUIRE(s["controllers"][1]["controller"] == 11);
}
TEST_CASE("SMF preflight rejects event track duration overflow and malformed "
          "meta before parser",
          "[song]") {
  auto valid = smf(bytes({0, 0x90, 60, 80, 1, 0x80, 60, 0, 0, 0xff, 0x2f, 0}));
  auto tracks = valid;
  tracks[10] = 1;
  tracks[11] = 1;
  REQUIRE_THROWS_WITH(importMidi(tracks, "Many"),
                      "MIDI needs 1–256 tracks; type 0 needs one.");
  std::string controls;
  controls.reserve(4000008);
  for (unsigned i = 0; i < 1000001; ++i)
    controls += bytes({0, 0xb0, 7, 100});
  controls += bytes({0, 0xff, 0x2f, 0});
  REQUIRE_THROWS_WITH(importMidi(smf(controls), "Many"),
                      "MIDI exceeds one million events.");
  auto longTrack = bytes({0, 0x90, 60, 80}) + vlq(14401) +
                   bytes({0x80, 60, 0, 0, 0xff, 0x2f, 0});
  REQUIRE_THROWS_WITH(importMidi(smf(longTrack, 0, 1), "Long"),
                      "MIDI exceeds two hours.");
  std::string overflow;
  for (unsigned i = 0; i < 9; ++i)
    overflow += vlq(0xfffffff) + bytes({0xb0, 7, 100});
  overflow += bytes({0, 0xff, 0x2f, 0});
  REQUIRE_THROWS_WITH(importMidi(smf(overflow), "Overflow"),
                      "MIDI tick range exceeded.");
  for (auto track :
       {bytes({0, 0xff, 0x51, 3, 0, 0, 0, 0, 0xff, 0x2f, 0}),
        bytes({0, 0xff, 0x51, 2, 1, 2, 0, 0xff, 0x2f, 0}),
        bytes({0, 0xff, 0x58, 4, 0, 2, 24, 8, 0, 0xff, 0x2f, 0}),
        bytes({0, 0xff, 0x59, 2, 8, 0, 0, 0xff, 0x2f, 0}),
        bytes({0, 0x90, 128, 90, 0, 0xff, 0x2f, 0}),
        bytes({0, 60, 90, 0, 0xff, 0x2f, 0}),
        bytes({0x81, 0x81, 0x81, 0x81, 0, 0x90, 60, 80, 0, 0xff, 0x2f, 0})})
    REQUIRE_THROWS_AS(importMidi(smf(track), "Malformed"),
                      std::invalid_argument);
}
TEST_CASE("SMF same-tick zero-length notes retain source event order",
          "[song]") {
  auto s = importMidi(
      smf(bytes({0, 0x90, 60, 90, 0, 0x80, 60, 0, 0, 0xb0, 64,   127,
                 1, 0x90, 62, 80, 1, 0x80, 62, 0, 0, 0xff, 0x2f, 0})),
      "Ordered");
  REQUIRE(s["notes"][0]["startOrder"].get<unsigned>() <
          s["notes"][0]["endOrder"].get<unsigned>());
  REQUIRE(s["controllers"][0]["order"].get<unsigned>() >
          s["notes"][0]["endOrder"].get<unsigned>());
}
