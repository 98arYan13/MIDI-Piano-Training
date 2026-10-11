#include <algorithm>
#include <cmath>
#include <sightreading/EarFlash.hpp>
#include <stdexcept>
namespace sightreading {
namespace {
const std::vector<std::string> intervals{"m2", "M2", "m3", "M3", "P4", "TT",
                                         "P5", "m6", "M6", "m7", "M7", "P8"};
const int steps[]{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
const std::vector<std::string> keys{"C", "G",  "D",  "A",  "E",  "B",
                                    "F", "Bb", "Eb", "Ab", "Db", "Gb"};
void require(bool ok, const std::string &message) {
  if (!ok)
    throw std::invalid_argument(message);
}
void number(const json &c, const char *field, int lo, int hi) {
  require(c.at(field).is_number_integer() && c[field].get<int>() >= lo &&
              c[field].get<int>() <= hi,
          std::string(field) + " must be " + std::to_string(lo) + " through " +
              std::to_string(hi) + ".");
}
void choice(const json &c, const char *field,
            const std::vector<std::string> &values) {
  require(c.at(field).is_string() &&
              std::find(values.begin(), values.end(),
                        c[field].get<std::string>()) != values.end(),
          std::string("Unsupported ") + field + ".");
}
void selection(const json &c, const char *field, const json &choices) {
  require(c.at(field).is_array() && !c[field].empty(),
          std::string("Select at least one ") + field + ".");
  std::set<json> seen;
  for (auto &v : c[field])
    require(std::find(choices.begin(), choices.end(), v) != choices.end() &&
                seen.insert(v).second,
            std::string("Invalid or repeated ") + field + " selection.");
}
json merge(json c, const json &p) {
  require(p.is_object(), "Settings must be an object.");
  for (auto i = p.begin(); i != p.end(); ++i) {
    require(c.contains(i.key()), "Unknown setting: " + i.key());
    if (c[i.key()].is_object())
      c[i.key()] = merge(c[i.key()], i.value());
    else
      c[i.key()] = i.value();
  }
  return c;
}
SpelledPitch chromatic(int midi) {
  return scalePitches("Chromatic", "major", midi, midi).at(0);
}
json notation(int midi) { return chromatic(midi).notation(); }
std::set<int> pitches(const json &values) {
  require(values.is_array() && !values.empty() && values.size() <= 4,
          "Answer requires one through four pitches.");
  std::set<int> result;
  for (auto &v : values) {
    require(v.is_number_integer(), "Pitch must be an integer.");
    int p = v;
    require(p >= 21 && p <= 108, "Pitch outside piano range.");
    result.insert(p);
  }
  require(result.size() == values.size(), "Answer pitches must be distinct.");
  return result;
}
} // namespace
json defaultEarConfig() {
  return {{"mode", "interval"},
          {"intervals", intervals},
          {"directions", {"up"}},
          {"root", 60},
          {"randomRoot", false},
          {"transpose", 0},
          {"tempo", 100},
          {"order", "ordered"},
          {"playOrder", "forward"},
          {"groups", 4},
          {"pitches", 1},
          {"range", {{"min", 48}, {"max", 84}}},
          {"melodyDirection", "any"},
          {"continuous", false},
          {"pitchWheel", false}};
}
json defaultFlashConfig() {
  return {{"mode", "noteMath"},
          {"roots", {"C", "D", "E", "F", "G", "A", "B"}},
          {"distances", {1, 2, 3, 4, 5, 6, 7}},
          {"keys", keys},
          {"inversions", {0, 1, 2}},
          {"staff", "treble"}};
}
json validateEarConfig(const json &c) {
  merge(defaultEarConfig(), c);
  choice(c, "mode", {"interval", "melody"});
  selection(c, "intervals", intervals);
  selection(c, "directions", {"up", "down"});
  number(c, "root", 36, 84);
  number(c, "transpose", -12, 12);
  number(c, "tempo", 20, 300);
  choice(c, "order", {"ordered", "random"});
  choice(c, "playOrder", {"forward", "reverse", "harmonic"});
  number(c, "groups", 2, 8);
  number(c, "pitches", 1, 4);
  choice(c, "melodyDirection", {"ascending", "descending", "any"});
  number(c.at("range"), "min", 21, 108);
  number(c.at("range"), "max", 21, 108);
  require(c["range"]["min"] <= c["range"]["max"],
          "Range minimum must not exceed maximum.");
  for (auto f : {"randomRoot", "continuous", "pitchWheel"})
    require(c.at(f).is_boolean(), std::string(f) + " must be boolean.");
  return c;
}
json validateFlashConfig(const json &c) {
  merge(defaultFlashConfig(), c);
  choice(c, "mode", {"noteMath", "chord"});
  selection(c, "roots", {"C", "D", "E", "F", "G", "A", "B"});
  selection(c, "distances", {1, 2, 3, 4, 5, 6, 7});
  selection(c, "keys", keys);
  selection(c, "inversions", {0, 1, 2});
  choice(c, "staff", {"treble", "bass", "grand"});
  return c;
}
json generateEar(const json &raw, unsigned seed, unsigned index, bool random) {
  auto c = validateEarConfig(raw);
  std::mt19937 rng(seed);
  int root = c["root"].get<int>() + c["transpose"].get<int>();
  if (c["randomRoot"] == true)
    root = 48 + int(rng() % 25) + c["transpose"].get<int>();
  json groups = json::array();
  json result = {{"root", root}, {"mode", c["mode"]}};
  if (c["mode"] == "interval") {
    auto interval = c["intervals"][(random || c["order"] == "random")
                                       ? rng() % c["intervals"].size()
                                       : index % c["intervals"].size()]
                        .get<std::string>();
    int semitones =
        steps[std::find(intervals.begin(), intervals.end(), interval) -
              intervals.begin()];
    auto direction =
        c["directions"][rng() % c["directions"].size()].get<std::string>();
    int target = root + (direction == "up" ? semitones : -semitones);
    require(root >= 21 && root <= 108 && target >= 21 && target <= 108,
            "Transposed interval is outside the piano range.");
    groups.push_back({root});
    groups.push_back({target});
    result["interval"] = interval;
    result["direction"] = direction;
    result["semitones"] = semitones;
  } else {
    // Major-root spelling follows the root pitch class, using the shared
    // theory.
    static const char *rootNames[]{"C",  "Db", "D",  "Eb", "E",  "F",
                                   "Gb", "G",  "Ab", "A",  "Bb", "B"};
    auto scale = scalePitches(rootNames[(root % 12 + 12) % 12], "major",
                              c["range"]["min"], c["range"]["max"]);
    int n = c["groups"], chord = c["pitches"];
    require(int(scale.size()) >= chord, "This range has too few major-scale "
                                        "pitches for the selected chord size.");
    int choices = int(scale.size()) - chord + 1;
    std::vector<int> starts;
    for (int i = 0; i < n; i++)
      starts.push_back(int(rng() % choices));
    if (c["melodyDirection"] == "ascending")
      std::sort(starts.begin(), starts.end());
    if (c["melodyDirection"] == "descending")
      std::sort(starts.begin(), starts.end(), std::greater<int>());
    for (int at : starts) {
      json g = json::array();
      for (int j = 0; j < chord; j++)
        g.push_back(scale[at + j].midi);
      groups.push_back(g);
    }
    result["key"] = rootNames[(root % 12 + 12) % 12];
  }
  result["groups"] = groups;
  return result;
}
json generateFlash(const json &raw, unsigned seed) {
  auto c = validateFlashConfig(raw);
  std::mt19937 rng(seed);
  if (c["mode"] == "noteMath") {
    auto root = c["roots"][rng() % c["roots"].size()].get<std::string>();
    int d = c["distances"][rng() % c["distances"].size()];
    std::string letters = "CDEFGAB";
    return {
        {"mode", "noteMath"},
        {"root", root},
        {"distance", d},
        {"answer", std::string(1, letters[(letters.find(root) + d - 1) % 7])}};
  }
  auto key = c["keys"][rng() % c["keys"].size()].get<std::string>();
  auto scale = scalePitches(key, "major", 48, 84);
  int at = int(rng() % 7);
  auto root = scale.at(at);
  int inversion = c["inversions"][rng() % c["inversions"].size()];
  std::vector<SpelledPitch> chord{scale.at(at), scale.at(at + 2),
                                  scale.at(at + 4)};
  int third = chord[1].midi - root.midi, fifth = chord[2].midi - root.midi;
  std::string quality = fifth == 6   ? "diminished"
                        : third == 3 ? "minor"
                                     : "major";
  for (int i = 0; i < inversion; i++) {
    auto p = chord.front();
    chord.erase(chord.begin());
    p.midi += 12;
    p.octave++;
    chord.push_back(p);
  }
  json notes = json::array();
  for (unsigned i = 0; i < chord.size(); i++)
    notes.push_back({{"id", std::to_string(i)},
                     {"midiNote", chord[i].midi},
                     {"spelling", chord[i].notation()},
                     {"startFrame", 0},
                     {"endFrame", 48000},
                     {"duration", "q"},
                     {"highlight", "target"}});
  std::string accidental = root.accidental == 0  ? "natural"
                           : root.accidental < 0 ? "flat"
                                                 : "sharp";
  return {{"mode", "chord"},
          {"key", key},
          {"inversion", inversion},
          {"notes", notes},
          {"answer",
           {{"root", std::string(1, char(std::toupper(root.letter)))},
            {"accidental", accidental},
            {"quality", quality}}}};
}
void EarFlash::stop() {
  if (!engine_.submit({EventType::Disconnect, Bus::Example, ExampleSource}))
    error_ = "Example cancellation used source-scoped queue-pressure recovery.";
  schedule_.clear();
  playing_ = paused_ = false;
  pauseRemaining_ = 0;
  attempt_.clear();
  ++revision_;
}
void EarFlash::next(bool random, uint64_t frame) {
  auto c = earConfig_;
  if (autoplay_)
    c["randomRoot"] = true;
  auto target = generateEar(c, seed_++, index_, random);
  stop();
  earTarget_ = std::move(target);
  ++index_;
  melodyIndex_ = 0;
  answered_ = false;
  feedback_.clear();
  error_.clear();
  exampleNotes_ = json::array();
  if (!elapsedOrigin_)
    elapsedOrigin_ = frame;
  ++revision_;
}
void EarFlash::play(const std::string &part, uint64_t frame) {
  if (earTarget_.is_null())
    next(false, frame);
  error_.clear();
  stop();
  melodyIndex_ = 0;
  answered_ = false;
  feedback_.clear();
  std::vector<std::vector<int>> groups =
      earTarget_["groups"].get<std::vector<std::vector<int>>>();
  if (part == "root")
    groups = {{earTarget_["root"]}};
  else if (part == "interval")
    groups = {groups.back()};
  else if (part == "motif") {
    require(earConfig_["mode"] == "interval",
            "Reference motifs are available for intervals.");
    int root = earTarget_["root"], target = earTarget_["groups"][1][0];
    groups = {{root}, {target}, {root}, {target}};
  }
  if (earConfig_["mode"] == "interval" && earConfig_["playOrder"] == "reverse")
    std::reverse(groups.begin(), groups.end());
  if (earConfig_["mode"] == "interval" &&
      earConfig_["playOrder"] == "harmonic") {
    std::set<int> all;
    for (auto &g : groups)
      all.insert(g.begin(), g.end());
    groups = {std::vector<int>(all.begin(), all.end())};
    if (part == "motif")
      groups.push_back(groups.front());
  }
  uint64_t beat = uint64_t(double(engine_.sampleRate()) * 60 /
                           earConfig_["tempo"].get<int>());
  uint64_t start = frame + 256;
  exampleNotes_ = json::array();
  schedule_.clear();
  for (unsigned i = 0; i < groups.size(); i++)
    for (int pitch : groups[i]) {
      uint64_t token = ++instance_;
      uint64_t at = start + i * beat;
      uint64_t off = at + uint64_t(beat * .8);
      schedule_.push_back({EventType::On, Bus::Example, ExampleSource, 0,
                           uint8_t(pitch), 90, token, at});
      schedule_.push_back({EventType::Off, Bus::Example, ExampleSource, 0,
                           uint8_t(pitch), 0, token, off});
      exampleNotes_.push_back({{"id", std::to_string(token)},
                               {"midiNote", pitch},
                               {"spelling", notation(pitch)},
                               {"startFrame", at},
                               {"endFrame", off},
                               {"duration", "q"},
                               {"highlight", "target"}});
    }
  std::stable_sort(schedule_.begin(), schedule_.end(),
                   [](auto &a, auto &b) { return a.frame < b.frame; });
  end_ = start + groups.size() * beat;
  playing_ = true;
  ++revision_;
  tick(frame);
  if (!playing_ && !error_.empty())
    throw std::runtime_error(error_);
}
void EarFlash::recoverInput() {
  attempt_.clear();
  pressed_.clear();
  for (auto &n : engine_.snapshot()["heldNotes"])
    if (n["bus"] == "live" && n["held"] == true)
      pressed_[{n["sourceId"].get<unsigned>(), n["channel"].get<unsigned>(),
                n["instance"].get<uint64_t>()}] = n["note"].get<int>();
}
void EarFlash::evaluate(uint64_t frame) {
  if (attempt_.empty() || playing_ || paused_ || earTarget_.is_null() ||
      answered_ || earConfig_["mode"] != "melody")
    return;
  auto expected = pitches(earTarget_["groups"][melodyIndex_]);
  std::set<int> actual;
  uint64_t first = UINT64_MAX, last = 0;
  bool wrong = false;
  for (auto [pitch, at] : attempt_) {
    actual.insert(pitch);
    first = std::min(first, at);
    last = std::max(last, at);
    wrong |= !expected.contains(pitch);
  }
  if (wrong || last - first > engine_.sampleRate() * 150 / 1000) {
    ++misses_;
    feedback_ = "Try this group again";
    attempt_.clear();
    ++revision_;
    return;
  }
  std::set<int> held;
  for (auto &[owner, pitch] : pressed_)
    held.insert(pitch);
  if (actual == expected && held == expected) {
    ++hits_;
    attempt_.clear();
    ++melodyIndex_;
    answered_ = melodyIndex_ >= earTarget_["groups"].size();
    feedback_ = answered_ ? "Melody correct" : "Group correct";
    ++revision_;
    if (answered_ && earConfig_["continuous"] == true) {
      next(true, frame);
      play("all", frame);
    }
  }
}
void EarFlash::input(const InputEvent &e) {
  if (e.type == EventType::Panic) {
    bent_.clear();
    pressed_.clear();
    stop();
    autoplay_ = false;
    return;
  }
  if (e.type == EventType::ResetLive) {
    pressed_.clear();
    attempt_.clear();
    return;
  }
  if (e.type == EventType::Disconnect) {
    if (e.source >= FirstReservedSource)
      return;
    bent_.erase(e.source);
    for (auto i = pressed_.begin(); i != pressed_.end();)
      if (std::get<0>(i->first) == e.source)
        i = pressed_.erase(i);
      else
        ++i;
    attempt_.clear();
    return;
  }
  if (e.bus != Bus::Live)
    return;
  if (e.type == EventType::On)
    updateExample(e.frame);
  if (e.type == EventType::PitchWheel) {
    int value = int(e.note) + (int(e.velocity) << 7);
    if (value >= 6000 && value <= 10000) {
      bent_.erase(e.source);
      return;
    }
    if (earConfig_["pitchWheel"] != true || bent_.contains(e.source))
      return;
    bent_.insert(e.source);
    try {
      if (value > 12000) {
        next(true, e.frame);
        play("all", e.frame);
      } else if (value < 4000)
        play("all", e.frame);
    } catch (const std::exception &ex) {
      error_ = ex.what();
      ++revision_;
    }
    return;
  }
  auto owner =
      std::tuple<unsigned, unsigned, uint64_t>{e.source, e.channel, e.instance};
  if (e.type == EventType::Off) {
    auto i = pressed_.find(owner);
    if (i != pressed_.end()) {
      int pitch = i->second;
      pressed_.erase(i);
      bool other = false;
      for (auto &[o, p] : pressed_)
        other |= p == pitch;
      if (!other)
        attempt_.erase(pitch);
    }
    return;
  }
  if (e.type != EventType::On || pressed_.contains(owner))
    return;
  pressed_[owner] = e.note;
  if (playing_ || paused_ || earConfig_["mode"] != "melody" ||
      earTarget_.is_null() || answered_)
    return;
  attempt_[e.note] = e.frame;
  try {
    evaluate(e.frame);
  } catch (const std::exception &ex) {
    stop();
    autoplay_ = false;
    error_ = ex.what();
    ++revision_;
  }
}
void EarFlash::updateExample(uint64_t frame) {
  if (playing_) {
    unsigned sent = 0;
    while (!schedule_.empty() &&
           schedule_.front().frame <= frame + engine_.sampleRate() / 4 &&
           sent++ < 128) {
      auto e = schedule_.front();
      if (!engine_.submit(e)) {
        stop();
        autoplay_ = false;
        error_ = "Example queue capacity reached. Stop and replay.";
        return;
      }
      schedule_.erase(schedule_.begin());
    }
    if (frame >= end_ && schedule_.empty()) {
      playing_ = false;
      attempt_.clear();
      ++revision_;
      if (autoplay_) {
        try {
          next(earConfig_["order"] == "random", frame);
          play("all", frame);
        } catch (const std::exception &ex) {
          stop();
          autoplay_ = false;
          error_ = ex.what();
          ++revision_;
        }
      }
    }
  }
}
void EarFlash::tick(uint64_t frame) {
  updateExample(frame);
  if (!playing_ && !attempt_.empty()) {
    auto first =
        std::min_element(attempt_.begin(), attempt_.end(),
                         [](auto &a, auto &b) { return a.second < b.second; })
            ->second;
    if (frame > first + engine_.sampleRate() * 150 / 1000) {
      ++misses_;
      attempt_.clear();
      feedback_ = "Incomplete chord — try again";
      ++revision_;
    }
  }
}
json EarFlash::ear(uint64_t frame) const {
  return {
      {"config", earConfig_},
      {"target", earTarget_},
      {"playing", playing_},
      {"paused", paused_},
      {"autoplay", autoplay_},
      {"answerLocked", playing_ || paused_},
      {"index", melodyIndex_},
      {"hits", hits_},
      {"misses", misses_},
      {"accuracy", hits_ + misses_ ? 100.0 * hits_ / (hits_ + misses_) : 0.0},
      {"elapsed", elapsedOrigin_
                      ? double(frame - elapsedOrigin_) / engine_.sampleRate()
                      : 0.0},
      {"feedback", feedback_},
      {"error", error_},
      {"answered", answered_},
      {"notes", exampleNotes_},
      {"endFrame", end_},
      {"pauseFrame", paused_ ? end_ - pauseRemaining_ : frame},
      {"revision", revision_}};
}
json EarFlash::flash() const {
  return {{"answerLocked", playing_ || paused_},
          {"config", flashConfig_},
          {"target", flashTarget_},
          {"hits", flashHits_},
          {"misses", flashMisses_},
          {"accuracy", flashHits_ + flashMisses_
                           ? 100.0 * flashHits_ / (flashHits_ + flashMisses_)
                           : 0.0},
          {"feedback", flashFeedback_},
          {"answered", flashAnswered_},
          {"revision", flashRevision_}};
}
json EarFlash::command(const std::string &type, const json &p, uint64_t frame) {
  require(p.is_object(), "Command params must be an object.");
  if (type == "ear.answer" || type == "flash.answer")
    updateExample(frame);
  if (type == "ear.option" || type == "flash.option") {
    auto field = p.at("field").get<std::string>();
    require(p.at("enabled").is_boolean(),
            "Selection toggle needs boolean enabled.");
    bool isEar = type == "ear.option";
    auto &c = isEar ? earConfig_ : flashConfig_;
    require((isEar && (field == "intervals" || field == "directions")) ||
                (!isEar && (field == "roots" || field == "distances" ||
                            field == "keys" || field == "inversions")),
            "Unsupported selection field.");
    auto values = c.at(field);
    auto at = std::find(values.begin(), values.end(), p.at("value"));
    if (p["enabled"] == true && at == values.end())
      values.push_back(p["value"]);
    if (p["enabled"] == false && at != values.end())
      values.erase(at);
    return command(isEar ? "ear.configure" : "flash.configure",
                   {{field, values}}, frame);
  }
  if (type.starts_with("flash.")) {
    if (type == "flash.configure") {
      auto c = validateFlashConfig(merge(flashConfig_, p));
      auto t = generateFlash(c, seed_);
      flashConfig_ = std::move(c);
      flashTarget_ = std::move(t);
      flashAnswered_ = false;
      flashFeedback_.clear();
      ++flashRevision_;
    } else if (type == "flash.next") {
      if (p.contains("seed")) {
        require(p["seed"].is_number_unsigned() || p["seed"].is_number_integer(),
                "Seed must be integer.");
        seed_ = p["seed"].get<unsigned>();
      }
      flashTarget_ = generateFlash(flashConfig_, seed_++);
      flashAnswered_ = false;
      flashFeedback_.clear();
      ++flashRevision_;
    } else if (type == "flash.answer") {
      require(!playing_ && !paused_, "Answers are disabled during examples.");
      require(!flashTarget_.is_null() && !flashAnswered_,
              "Choose Next for a fresh question.");
      auto answer = p.at("answer");
      bool correct = answer == flashTarget_["answer"];
      if (correct) {
        ++flashHits_;
        flashAnswered_ = true;
        flashFeedback_ = "Correct";
      } else {
        ++flashMisses_;
        flashFeedback_ = "Try again";
      }
      ++flashRevision_;
    } else if (type == "flash.reset") {
      flashHits_ = flashMisses_ = 0;
      flashFeedback_.clear();
      ++flashRevision_;
    } else
      throw std::invalid_argument("Unknown Flash command.");
    return flash();
  }
  if (type == "ear.configure") {
    auto c = validateEarConfig(merge(earConfig_, p));
    generateEar(c, seed_);
    stop();
    autoplay_ = false;
    earConfig_ = std::move(c);
    next(false, frame);
  } else if (type == "ear.next" || type == "ear.random" || type == "ear.skip" ||
             type == "ear.melody") {
    if (p.contains("seed")) {
      require(p["seed"].is_number_unsigned() || p["seed"].is_number_integer(),
              "Seed must be integer.");
      seed_ = p["seed"].get<unsigned>();
    }
    next(type != "ear.next", frame);
    if (autoplay_ || type == "ear.melody")
      play("all", frame);
  } else if (type == "ear.play" || type == "ear.replay" || type == "ear.root" ||
             type == "ear.interval" || type == "ear.motif") {
    if (paused_ && type == "ear.play") {
      uint64_t shift = frame + 256;
      schedule_.clear();
      for (auto &n : exampleNotes_) {
        uint64_t oldStart = n["startFrame"], oldEnd = n["endFrame"];
        if (oldEnd <= end_ - pauseRemaining_)
          continue;
        uint64_t start = shift + (oldStart > end_ - pauseRemaining_
                                      ? oldStart - (end_ - pauseRemaining_)
                                      : 0),
                 off = shift + oldEnd - (end_ - pauseRemaining_);
        uint64_t token = ++instance_;
        int pitch = n["midiNote"];
        schedule_.push_back({EventType::On, Bus::Example, ExampleSource, 0,
                             uint8_t(pitch), 90, token, start});
        schedule_.push_back({EventType::Off, Bus::Example, ExampleSource, 0,
                             uint8_t(pitch), 0, token, off});
        n["startFrame"] = start;
        n["endFrame"] = off;
      }
      std::stable_sort(schedule_.begin(), schedule_.end(),
                       [](auto &a, auto &b) { return a.frame < b.frame; });
      end_ = shift + pauseRemaining_;
      paused_ = false;
      playing_ = true;
      ++revision_;
      tick(frame);
      if (!playing_ && !error_.empty())
        throw std::runtime_error(error_);
    } else
      play(type == "ear.root"       ? "root"
           : type == "ear.interval" ? "interval"
           : type == "ear.motif"    ? "motif"
                                    : "all",
           frame);
  } else if (type == "ear.pause") {
    if (playing_) {
      auto remaining = end_ > frame ? end_ - frame : 0;
      auto oldEnd = end_;
      stop();
      end_ = oldEnd;
      pauseRemaining_ = remaining;
      paused_ = true;
      ++revision_;
    }
  } else if (type == "ear.stop") {
    stop();
    autoplay_ = false;
  } else if (type == "ear.autoplay") {
    require(p.contains("enabled") && p["enabled"].is_boolean(),
            "Autoplay requires boolean enabled.");
    autoplay_ = p["enabled"];
    if (autoplay_)
      play("all", frame);
    else
      stop();
  } else if (type == "ear.answer") {
    require(!playing_ && !paused_, "Answers are disabled during examples.");
    require(!earTarget_.is_null() && !answered_,
            "Choose New for a fresh question.");
    if (earConfig_["mode"] == "interval") {
      auto answer = p.at("interval");
      require(answer.is_string(), "Choose an interval answer.");
      if (answer == earTarget_["interval"]) {
        ++hits_;
        answered_ = true;
        feedback_ = "Correct";
      } else {
        ++misses_;
        feedback_ = "Try again";
      }
      ++revision_;
    } else {
      auto actual = pitches(p.at("pitches"));
      auto expected = pitches(earTarget_["groups"][melodyIndex_]);
      if (actual == expected) {
        ++hits_;
        ++melodyIndex_;
        answered_ = melodyIndex_ >= earTarget_["groups"].size();
        feedback_ = answered_ ? "Melody correct" : "Group correct";
      } else {
        ++misses_;
        feedback_ = "Try this group again";
      }
      ++revision_;
      if (answered_ && earConfig_["continuous"] == true) {
        next(true, frame);
        play("all", frame);
      }
    }
  } else if (type == "ear.reset") {
    hits_ = misses_ = 0;
    elapsedOrigin_ = frame;
    feedback_.clear();
    ++revision_;
  } else
    throw std::invalid_argument("Unknown Ear command.");
  return ear(frame);
}
} // namespace sightreading
