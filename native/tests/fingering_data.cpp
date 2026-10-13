#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <algorithm>
#include <filesystem>
#include <sightreading/FingeringData.hpp>
#include <sightreading/Storage.hpp>

using namespace sightreading;
using json = nlohmann::json;

static json fixture() {
  return {{"id", "finger"}, {"title", "Before"}, {"ppq", 480},
          {"tracks", json::array({
              {{"id", 0}, {"title", "Track 1"}, {"staff", "split"}, {"percussion", false}},
              {{"id", 1}, {"title", "Piano Backing"}, {"staff", "grand"}, {"percussion", false}},
              {{"id", 2}, {"title", "Drums"}, {"staff", "split"}, {"percussion", true}}})},
          {"notes", json::array({
              {{"id", 1}, {"trackId", 0}, {"midiNote", 55}, {"startTick", 0}, {"endTick", 240}},
              {{"id", 2}, {"trackId", 0}, {"midiNote", 65}, {"startTick", 240}, {"endTick", 480}},
              {{"id", 3}, {"trackId", 1}, {"midiNote", 70}, {"startTick", 480}, {"endTick", 720}},
              {{"id", 4}, {"trackId", 2}, {"midiNote", 60}, {"startTick", 0}, {"endTick", 120}},
              {{"id", 5}, {"trackId", 0}, {"midiNote", 20}, {"startTick", 0}, {"endTick", 120}}})}};
}

static json model() {
  return {{"name", "ArGNN"}, {"revision", "abc123"},
          {"checkpointHashes", {{"left", std::string(64, 'a')}, {"right", std::string(64, 'b')}}}};
}

static json resultFor(const json &song) {
  json assignments = json::array();
  for (auto &n : song["notes"]) {
    if (n["id"] == 4 || n["id"] == 5) continue;
    assignments.push_back({{"id", n["id"]}, {"finger", int(n["id"])},
                           {"hand", resolvedFingeringHand(song, n)}});
  }
  return {{"model", model()}, {"assignments", assignments}};
}

TEST_CASE("Fingering hand mapping follows renderer precedence", "[fingering]") {
  auto s = fixture();
  REQUIRE(resolvedFingeringHand(s, s["notes"][0]) == "left");
  REQUIRE(resolvedFingeringHand(s, s["notes"][1]) == "right");
  REQUIRE(resolvedFingeringHand(s, s["notes"][2]) == "left");
  s["tracks"][0]["staff"] = "bass";
  REQUIRE(resolvedFingeringHand(s, s["notes"][1]) == "left");
  s["tracks"][0]["staff"] = "treble";
  s["tracks"][0]["title"] = "Left Hand";
  REQUIRE(resolvedFingeringHand(s, s["notes"][0]) == "right");
  s["tracks"][0]["staff"] = "split";
  REQUIRE(resolvedFingeringHand(s, s["notes"][0]) == "left");
  s["tracks"][0]["title"] = "Track 1";
  REQUIRE(resolvedFingeringHand(s, s["notes"][1]) == "right");
  s["tracks"][0]["title"] = "RH";
  REQUIRE(resolvedFingeringHand(s, s["notes"][0]) == "right");
  s["tracks"][0]["title"] = "LH";
  REQUIRE(resolvedFingeringHand(s, s["notes"][1]) == "left");
  s["tracks"][0]["title"] = "Practice Melody";
  REQUIRE(resolvedFingeringHand(s, s["notes"][0]) == "right");
  s["tracks"][0]["title"] = "Track 1";
  s["notes"][0]["staff"] = "treble";
  REQUIRE(resolvedFingeringHand(s, s["notes"][0]) == "right");
}

TEST_CASE("Fingering request excludes percussion and unsupported range", "[fingering]") {
  auto request = fingeringRequest(fixture(), "job");
  REQUIRE(request["version"] == 1);
  REQUIRE(request["notes"].size() == 3);
  REQUIRE(request["notes"][0]["id"] == 1);
  REQUIRE(request["notes"][0]["hand"] == "left");
}

TEST_CASE("Fingering fingerprint ignores title and changes with anchors", "[fingering]") {
  auto s = fixture();
  const auto initial = fingeringFingerprint(s);
  s["title"] = "After";
  REQUIRE(fingeringFingerprint(s) == initial);
  s["notes"][0]["fingering"] = {{"finger", 2}, {"hand", "left"}, {"source", "manual"}};
  REQUIRE(fingeringFingerprint(s) != initial);
  auto anchored = fingeringFingerprint(s);
  s["tracks"][0]["staff"] = "bass";
  REQUIRE(fingeringFingerprint(s) != anchored);
}

TEST_CASE("Generated fingering commits atomically and survives restart", "[fingering]") {
  auto dir = std::filesystem::temp_directory_path() / ("sr-fingering-" +
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  const auto original = std::string("midi\0bytes", 10);
  auto s = fixture();
  {
    Storage storage(dir);
    storage.putSong(s, original);
    const auto fingerprint = fingeringFingerprint(s);
    auto result = resultFor(s);
    auto saved = storage.commitFingering("finger", fingerprint, result);
    REQUIRE(saved["fingeringRevision"] == 1);
    REQUIRE(storage.song("finger")["notes"][0]["fingering"]["finger"] == 1);
    REQUIRE(storage.originalSong("finger") == original);
    REQUIRE(saved["fingeringRun"]["unsupported"] == 2);
    REQUIRE(saved["notes"][0]["startTick"] == s["notes"][0]["startTick"]);
    REQUIRE(saved["notes"][0]["midiNote"] == s["notes"][0]["midiNote"]);
    REQUIRE(saved["notes"][0]["endTick"] == s["notes"][0]["endTick"]);
    REQUIRE(storage.songs()[0]["fingeringRevision"] == 1);
    REQUIRE_FALSE(storage.songs()[0].contains("notes"));
    REQUIRE_THROWS(storage.commitFingering("finger", fingerprint,
        {{"model", model()}, {"assignments", json::array()}}));
    REQUIRE(storage.song("finger")["fingeringRevision"] == 1);
    REQUIRE_THROWS(storage.editFingering("finger", {1}, 0));
    REQUIRE_THROWS(storage.editFingering("finger", {1, 1}, 3));
    REQUIRE_THROWS(storage.editFingering("finger", {999}, 3));
    REQUIRE(storage.song("finger")["fingeringRevision"] == 1);
    storage.editFingering("finger", {1}, 3);
    auto edited = storage.song("finger");
    REQUIRE(edited["fingeringRevision"] == 2);
    REQUIRE(edited["notes"][0]["fingering"]["source"] == "manual");
    REQUIRE(edited["notes"][0]["fingering"]["automaticFinger"] == 1);
    storage.editFingering("finger", {1}, std::nullopt);
    REQUIRE(storage.song("finger")["notes"][0]["fingering"]["finger"] == 1);
    REQUIRE(storage.song("finger")["fingeringRevision"] == 3);
  }
  {
    Storage storage(dir);
    REQUIRE(storage.song("finger")["notes"][0]["fingering"]["finger"] == 1);
    REQUIRE(storage.originalSong("finger") == original);
  }
  std::filesystem::remove_all(dir);
}

TEST_CASE("Fingering commit rejects stale or trashed song", "[fingering]") {
  auto dir = std::filesystem::temp_directory_path() / ("sr-fingering-stale-" +
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  auto s = fixture();
  {
    Storage storage(dir);
    storage.putSong(s, "midi");
    auto fp = fingeringFingerprint(s);
    storage.patchSong("finger", {{"title", "renamed"}});
    REQUIRE_NOTHROW(storage.commitFingering("finger", fp, resultFor(s)));
    storage.trashSong("finger", true);
    REQUIRE_THROWS(storage.commitFingering("finger", fingeringFingerprint(storage.song("finger")), resultFor(s)));
  }
  std::filesystem::remove_all(dir);
}

TEST_CASE("Tip density classifications use exact thresholds", "[fingering]") {
  auto s = fixture();
  for (auto &n : s["notes"]) if (n["id"] != 4 && n["id"] != 5)
    n["fingering"] = {{"finger", 1}, {"hand", resolvedFingeringHand(s, n)}, {"source", "generated"}};
  assignFingeringTips(s);
  REQUIRE(s["notes"][0]["fingering"]["tips"].is_array());
  REQUIRE(std::find(s["notes"][0]["fingering"]["tips"].begin(),
                    s["notes"][0]["fingering"]["tips"].end(), "phraseStart") !=
          s["notes"][0]["fingering"]["tips"].end());
}

TEST_CASE("Tip classifier uses phrase and white key boundaries", "[fingering]") {
  json s = {{"ppq", 480}, {"tracks", json::array({{{"id", 0}, {"title", "Track 1"}, {"staff", "split"}}})},
    {"notes", json::array({
      {{"id", 1}, {"trackId", 0}, {"midiNote", 60}, {"startTick", 0}, {"endTick", 100}, {"fingering", {{"finger", 3}, {"source", "generated"}, {"hand", "right"}}}},
      {{"id", 2}, {"trackId", 0}, {"midiNote", 64}, {"startTick", 100}, {"endTick", 200}, {"fingering", {{"finger", 3}, {"source", "generated"}, {"hand", "right"}}}},
      {{"id", 3}, {"trackId", 0}, {"midiNote", 72}, {"startTick", 200}, {"endTick", 300}, {"fingering", {{"finger", 3}, {"source", "generated"}, {"hand", "right"}}}},
      {{"id", 4}, {"trackId", 0}, {"midiNote", 72}, {"startTick", 779}, {"endTick", 779}, {"fingering", {{"finger", 3}, {"source", "generated"}, {"hand", "right"}}}},
      {{"id", 5}, {"trackId", 0}, {"midiNote", 74}, {"startTick", 1259}, {"endTick", 1300}, {"fingering", {{"finger", 3}, {"source", "generated"}, {"hand", "right"}}}}})}};
  assignFingeringTips(s);
  auto tips = [&](size_t i) { return s["notes"][i]["fingering"]["tips"]; };
  auto has = [&](size_t i, const char *name) { auto value = tips(i); return std::find(value.begin(), value.end(), name) != value.end(); };
  REQUIRE(has(1, "positionChange"));
  REQUIRE(has(2, "majorPositionChange"));
  REQUIRE_FALSE(has(3, "phraseStart"));
  REQUIRE(has(4, "phraseStart"));
}

TEST_CASE("Tip classifier marks thumb crossings and difficult chords", "[fingering]") {
  json s = {{"ppq", 480}, {"tracks", json::array({{{"id", 0}, {"title", "Track 1"}, {"staff", "split"}}})},
    {"notes", json::array({
      {{"id", 1}, {"trackId", 0}, {"midiNote", 60}, {"startTick", 0}, {"endTick", 100}, {"fingering", {{"finger", 4}, {"source", "generated"}, {"hand", "right"}}}},
      {{"id", 2}, {"trackId", 0}, {"midiNote", 62}, {"startTick", 100}, {"endTick", 200}, {"fingering", {{"finger", 1}, {"source", "generated"}, {"hand", "right"}}}},
      {{"id", 3}, {"trackId", 0}, {"midiNote", 48}, {"startTick", 300}, {"endTick", 400}, {"fingering", {{"finger", 2}, {"source", "generated"}, {"hand", "left"}}}},
      {{"id", 4}, {"trackId", 0}, {"midiNote", 53}, {"startTick", 300}, {"endTick", 400}, {"fingering", {{"finger", 3}, {"source", "generated"}, {"hand", "left"}}}},
      {{"id", 5}, {"trackId", 0}, {"midiNote", 57}, {"startTick", 300}, {"endTick", 400}, {"fingering", {{"finger", 4}, {"source", "generated"}, {"hand", "left"}}}},
      {{"id", 6}, {"trackId", 0}, {"midiNote", 58}, {"startTick", 300}, {"endTick", 400}, {"fingering", {{"finger", 5}, {"source", "generated"}, {"hand", "left"}}}},
      {{"id", 7}, {"trackId", 0}, {"midiNote", 48}, {"startTick", 500}, {"endTick", 600}, {"fingering", {{"finger", 2}, {"source", "generated"}, {"hand", "left"}}}},
      {{"id", 8}, {"trackId", 0}, {"midiNote", 60}, {"startTick", 500}, {"endTick", 600}, {"staff", "bass"}, {"fingering", {{"finger", 5}, {"source", "generated"}, {"hand", "left"}}}}})}};
  assignFingeringTips(s);
  const auto &cross = s["notes"][1]["fingering"]["tips"];
  REQUIRE(std::find(cross.begin(), cross.end(), "thumbCrossing") != cross.end());
  for (size_t i = 2; i < 6; ++i) {
    const auto &tips = s["notes"][i]["fingering"]["tips"];
    REQUIRE(std::find(tips.begin(), tips.end(), "difficultChord") != tips.end());
  }
  for (size_t i = 6; i < 8; ++i) {
    const auto &tips = s["notes"][i]["fingering"]["tips"];
    REQUIRE(std::find(tips.begin(), tips.end(), "difficultChord") != tips.end());
  }
}

TEST_CASE("Invalid fingering result rolls back malformed assignments atomically", "[fingering]") {
  auto dir = std::filesystem::temp_directory_path() / ("sr-fingering-invalid-" +
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  {
    Storage storage(dir);
    auto s = fixture();
    storage.putSong(s, "original");
    const auto fp = fingeringFingerprint(s);
    const auto baseline = storage.song("finger");
    auto valid = resultFor(s);
    auto duplicate = valid; duplicate["assignments"].push_back(duplicate["assignments"][0]);
    auto unknown = valid; unknown["assignments"][0]["id"] = 999;
    auto missing = valid; missing["assignments"].erase(missing["assignments"].begin());
    auto badDigit = valid; badDigit["assignments"][0]["finger"] = 6;
    auto badHand = valid; badHand["assignments"][0]["hand"] = "both";
    auto badSource = valid; badSource["assignments"][0]["source"] = "manual";
    auto badModel = valid; badModel["model"]["name"] = "other";
    for (const auto &invalid : {duplicate, unknown, missing, badDigit, badHand, badSource, badModel}) {
      REQUIRE_THROWS(storage.commitFingering("finger", fp, invalid));
      REQUIRE(storage.song("finger") == baseline);
    }
  }
  std::filesystem::remove_all(dir);
}

TEST_CASE("Track edits preserve saved digits and invalidate hand mapping revision", "[fingering]") {
  auto dir = std::filesystem::temp_directory_path() / ("sr-fingering-track-" +
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  {
    Storage storage(dir);
    auto s = fixture();
    storage.putSong(s, "original");
    storage.commitFingering("finger", fingeringFingerprint(s), resultFor(s));
    const auto before = storage.song("finger");
    const auto fingerprint = fingeringFingerprint(before);
    storage.patchSong("finger", {{"tracks", json::array({{{"id", 0}, {"staff", "treble"}}})}});
    const auto after = storage.song("finger");
    REQUIRE(fingeringFingerprint(after) != fingerprint);
    REQUIRE(after["fingeringRevision"] == before["fingeringRevision"].get<uint64_t>() + 1);
    REQUIRE(after["notes"][0]["fingering"]["finger"] == before["notes"][0]["fingering"]["finger"]);
    REQUIRE(after["notes"][0]["fingering"]["hand"] == "right");
  }
  std::filesystem::remove_all(dir);
}
