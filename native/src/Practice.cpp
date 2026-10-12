#include <algorithm>
#include <cmath>
#include <sightreading/Practice.hpp>
#include <stdexcept>
namespace sightreading {
namespace {
const std::vector<std::string> keys{"C",  "G",  "D",        "A",  "E",
                                    "B",  "F",  "Bb",       "Eb", "Ab",
                                    "Db", "Gb", "Chromatic"},
    scales{"major", "naturalMinor", "harmonicMinor", "melodicMinor"},
    generators{"random",   "triads",    "sevenths",   "progression",
               "position", "intervals", "chordSymbol"};
int mod(int n, int m) { return (n % m + m) % m; }
void choice(const json &c, const char *f, const std::vector<std::string> &a) {
  if (!c[f].is_string() ||
      std::find(a.begin(), a.end(), c[f].get<std::string>()) == a.end())
    throw std::invalid_argument(std::string("Unsupported ") + f + ".");
}
void integer(const json &c, const char *f, int lo, int hi) {
  const auto &value = c[f];
  const bool valid = value.is_number_unsigned()
      ? value.get<uint64_t>() <= uint64_t(hi) &&
            (lo <= 0 || value.get<uint64_t>() >= uint64_t(lo))
      : value.is_number_integer() && value.get<int64_t>() >= lo &&
            value.get<int64_t>() <= hi;
  if (!valid)
    throw std::invalid_argument(std::string(f) + " must be " +
                                std::to_string(lo) + "–" + std::to_string(hi) +
                                ".");
}
void known(const json &p, const json &b) {
  if (!p.is_object())
    throw std::invalid_argument("Settings must be an object.");
  for (auto i = p.begin(); i != p.end(); ++i)
    if (!b.contains(i.key()))
      throw std::invalid_argument("Unknown setting: " + i.key());
}
json merged(json b, const json &p) {
  known(p, b);
  for (auto i = p.begin(); i != p.end(); ++i) {
    if (b[i.key()].is_object())
      b[i.key()] = merged(b[i.key()], i.value());
    else
      b[i.key()] = i.value();
  }
  return b;
}
std::set<int> classes(const PracticeTarget &t, int ceiling) {
  std::set<int> s;
  for (auto p : t.pitches)
    if (p.midi <= ceiling)
      s.insert(mod(p.midi, 12));
  return s;
}
std::string pitchName(SpelledPitch p) {
  std::string n(1, char(std::toupper(p.letter)));
  if (p.accidental > 0)
    n += p.accidental == 2 ? "##" : "#";
  if (p.accidental < 0)
    n += p.accidental == -2 ? "bb" : "b";
  return n;
}
} // namespace
json SpelledPitch::notation() const {
  static const char *a[]{"bb", "b", "n", "#", "##"};
  return {{"letter", std::string(1, letter)},
          {"accidental", a[std::clamp(accidental, -2, 2) + 2]},
          {"octave", octave}};
}
std::vector<SpelledPitch> scalePitches(const std::string &key,
                                       const std::string &scale, int low,
                                       int high) {
  if (std::find(keys.begin(), keys.end(), key) == keys.end() ||
      std::find(scales.begin(), scales.end(), scale) == scales.end())
    throw std::invalid_argument("Select a supported key and scale.");
  const std::string letters = "cdefgab";
  const int natural[]{0, 2, 4, 5, 7, 9, 11};
  std::vector<SpelledPitch> out;
  if (key == "Chromatic") {
    const char *ls = "ccddeffggaab";
    const int ac[]{0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
    for (int m = low; m <= high; ++m)
      out.push_back({m, ls[mod(m, 12)], ac[mod(m, 12)], m / 12 - 1});
    return out;
  }
  int root = int(letters.find(char(std::tolower(key[0])))),
      pc = natural[root] + (key.size() > 1 ? (key[1] == 'b' ? -1 : 1) : 0);
  std::vector<int> steps = scale == "major"
                               ? std::vector<int>{0, 2, 4, 5, 7, 9, 11}
                               : std::vector<int>{0, 2, 3, 5, 7, 8, 10};
  if (scale == "harmonicMinor")
    steps[6] = 11;
  if (scale == "melodicMinor") {
    steps[5] = 9;
    steps[6] = 11;
  }
  for (int oct = -1; oct <= 9; ++oct)
    for (int d = 0; d < 7; ++d) {
      int m = (oct + 1) * 12 + pc + steps[d], diatonic = root + d,
          so = oct + diatonic / 7, l = diatonic % 7,
          acc = m - ((so + 1) * 12 + natural[l]);
      if (m >= low && m <= high)
        out.push_back({m, letters[l], acc, so});
    }
  std::sort(out.begin(), out.end(),
            [](auto a, auto b) { return a.midi < b.midi; });
  return out;
}
json PracticeTarget::jsonValue() const {
  json p = json::array();
  for (auto n : pitches)
    p.push_back({{"midiNote", n.midi}, {"spelling", n.notation()}});
  return {{"pitches", p}, {"symbol", symbol}, {"fingering", fingering}};
}
json defaultPracticeConfig() {
  return {{"staff", "treble"},
          {"hints", false},
          {"hand", "both"},
          {"generator", "random"},
          {"key", "C"},
          {"range", {{"min", 48}, {"max", 84}}},
          {"smoothness", 3},
          {"mode", "wait"},
          {"tempo", 100},
          {"toleranceMs", 150},
          {"seed", 1},
          {"generatorOptions",
           {{"notes", 1},
            {"hands", 2},
            {"chordBased", false},
            {"inversions", true},
            {"openVoicing", false},
            {"progression", "autumnLeaves"},
            {"startingFinger", 1},
            {"intervals", json::array({2, 3, 4, 5, 6, 7})},
            {"randomChords", true},
            {"multiKey", false},
            {"keys", json::array({"C"})},
            {"scale", "major"},
            {"tones", 3},
            {"commonNotes", 0},
            {"ignoreAbove", 108}}}};
}
json validatePracticeConfig(const json &c) {
  known(c, defaultPracticeConfig());
  if (!c.at("hints").is_boolean())
    throw std::invalid_argument("hints must be boolean.");
  choice(c, "staff", {"treble", "bass", "grand", "chordSymbol"});
  choice(c, "hand", {"both", "left", "right"});
  choice(c, "generator", generators);
  choice(c, "key", keys);
  choice(c, "mode", {"wait", "scroll"});
  integer(c, "smoothness", 0, 6);
  integer(c, "tempo", 20, 300);
  integer(c, "toleranceMs", 20, 1000);
  integer(c, "seed", 0, 2147483647);
  const auto &r = c["range"];
  known(r, defaultPracticeConfig()["range"]);
  integer(r, "min", 21, 108);
  integer(r, "max", 21, 108);
  if (r["min"].get<int>() > r["max"].get<int>())
    throw std::invalid_argument("Minimum range must not exceed maximum range.");
  if ((c["hand"] == "left" && r["min"].get<int>() >= 60) ||
      (c["hand"] == "right" && r["max"].get<int>() < 60))
    throw std::invalid_argument("Selected hand has no notes in this range.");
  const auto &o = c["generatorOptions"];
  known(o, defaultPracticeConfig()["generatorOptions"]);
  integer(o, "notes", 1, 5);
  integer(o, "hands", 1, 2);
  integer(o, "startingFinger", 1, 5);
  integer(o, "tones", 3, 4);
  integer(o, "commonNotes", 0, 2);
  integer(o, "ignoreAbove", 21, 108);
  choice(o, "scale", scales);
  choice(o, "progression",
         {"autumnLeaves", "fifties", "circle", "substitution"});
  for (auto f :
       {"chordBased", "inversions", "openVoicing", "randomChords", "multiKey"})
    if (!o[f].is_boolean())
      throw std::invalid_argument(std::string(f) + " must be boolean.");
  if (!o["intervals"].is_array())
    throw std::invalid_argument("Intervals must be a list.");
  for (auto i : o["intervals"])
    if (!i.is_number_integer() || i.get<int>() < 2 || i.get<int>() > 7)
      throw std::invalid_argument("Intervals must be 2nd through 7th.");
  if (c["generator"] == "intervals" && o["intervals"].empty())
    throw std::invalid_argument(
        "Select at least one interval (2nd through 7th).");
  if (!o["keys"].is_array() || o["keys"].size() > 12)
    throw std::invalid_argument("Select up to 12 chord keys.");
  for (auto k : o["keys"])
    if (!k.is_string() || std::find(keys.begin(), keys.end() - 1,
                                    k.get<std::string>()) == keys.end() - 1)
      throw std::invalid_argument("Chord keys must be named tonal keys.");
  if (o["multiKey"] == true && o["keys"].empty())
    throw std::invalid_argument("Select at least one chord key.");
  return c;
}
std::vector<PracticeTarget> generatePractice(const json &raw, unsigned seed,
                                             unsigned count) {
  const auto c = validatePracticeConfig(raw);
  const auto &o = c["generatorOptions"];
  int low = c["range"]["min"], high = c["range"]["max"], hands = o["hands"],
      smooth = c["smoothness"];
  if (c["hand"] == "left") high = std::min(high, 59);
  if (c["hand"] == "right") low = std::max(low, 60);
  if (c["hand"] != "both") hands = 1;
  std::string g = c["staff"] == "chordSymbol"
                      ? "chordSymbol"
                      : c["generator"].get<std::string>();
  std::mt19937 rng(seed);
  std::vector<PracticeTarget> out;
  auto random = [&](unsigned n) { return unsigned(rng() % n); };
  std::string generationKey = c["key"];
  if (g == "progression" && generationKey == "Chromatic")
    generationKey = keys[random(12)];
  if (g == "position") {
    std::string key = c["key"];
    if (key == "Chromatic")
      key = keys[random(12)];
    auto scale = scalePitches(key, o["scale"], low, high);
    if (scale.size() < 5)
      throw std::invalid_argument(
          "Five-finger position needs five diatonic notes. Widen the range.");
    const auto start = random(unsigned(scale.size() - 4));
    for (unsigned step = 0; step < count; ++step) {
      bool found = false;
      for (unsigned search = 0; search < 4096 && !found; ++search) {
        unsigned finger =
            step == 0 ? o["startingFinger"].get<unsigned>() - 1 : random(5);
        auto pitch = scale[start + finger];
        if (!out.empty() && smooth &&
            std::abs(pitch.midi - out.back().pitches[0].midi) > 14 - 2 * smooth)
          continue;
        PracticeTarget target;
        target.pitches.push_back(pitch);
        target.fingering = int(finger) + 1;
        out.push_back(std::move(target));
        found = true;
      }
      if (!found)
        throw std::invalid_argument("No smooth target within this five-finger "
                                    "position. Reduce smoothness.");
    }
    return out;
  }
  for (unsigned step = 0; step < count; ++step) {
    bool found = false;
    for (unsigned search = 0; search < 4096 && !found; ++search) {
      std::string key = generationKey;
      if (g == "chordSymbol" && o["multiKey"] == true)
        key = o["keys"][random(unsigned(o["keys"].size()))];
      if (key == "Chromatic" && (g != "random" || o["chordBased"] == true))
        key = keys[random(12)];
      auto scale = scalePitches(key, o["scale"], 0, 132);
      std::vector<unsigned> pool;
      for (unsigned i = 0; i < scale.size(); ++i)
        if (scale[i].midi >= low && scale[i].midi <= high)
          pool.push_back(i);
      if (pool.empty())
        throw std::invalid_argument(
            "No notes in this key/range. Widen the range or choose Chromatic.");
      PracticeTarget t;
      unsigned root = pool[random(unsigned(pool.size()))];
      auto add = [&](unsigned i) {
        if (i < scale.size())
          t.pitches.push_back(scale[i]);
      };
      if (g == "random") {
        if (o["chordBased"] == true) {
          pool.clear();
          for (auto d : {0u, 2u, 4u, 6u, 7u})
            if (root + d < scale.size())
              pool.push_back(root + d);
        }
        for (int i = 0; i < o["notes"].get<int>(); ++i)
          add(pool[random(unsigned(pool.size()))]);
      } else if (g == "intervals") {
        add(root);
        add(root +
            o["intervals"][random(unsigned(o["intervals"].size()))]
                .get<unsigned>() -
            1);
      } else {
        unsigned tones = g == "sevenths"      ? 4
                         : g == "chordSymbol" ? o["tones"].get<unsigned>()
                                              : 3;
        if (g == "progression") {
          std::vector<unsigned> degrees =
              o["progression"] == "autumnLeaves"
                  ? std::vector<unsigned>{1, 4, 0, 3, 6, 2, 5}
              : o["progression"] == "fifties"
                  ? std::vector<unsigned>{0, 5, 3, 4}
              : o["progression"] == "circle"
                  ? std::vector<unsigned>{0, 3, 6, 2, 5, 1, 4}
                  : std::vector<unsigned>{0, 5, 1, 4};
          std::vector<unsigned> roots;
          char tonic = char(std::tolower(key[0]));
          for (unsigned i = 0; i < scale.size(); ++i)
            if (scale[i].letter == tonic &&
                i + degrees[step % degrees.size()] < scale.size())
              roots.push_back(i + degrees[step % degrees.size()]);
          root = roots[random(unsigned(roots.size()))];
        }
        if (g == "progression" && o["progression"] == "substitution" &&
            step % 4 == 3) {
          const char *names[]{"C",  "Db", "D",  "Eb", "E",  "F",
                              "Gb", "G",  "Ab", "A",  "Bb", "B"};
          int pc = mod(scale[root].midi + 6, 12);
          key = names[pc];
          scale = scalePitches(key, "major", 0, 132);
          std::vector<unsigned> roots;
          for (unsigned i = 0; i < scale.size(); ++i)
            if (mod(scale[i].midi, 12) == pc)
              roots.push_back(i);
          root = roots[random(unsigned(roots.size()))];
        }
        if (g == "chordSymbol" && o["randomChords"] == false) {
          while (root > 0 && scale[root].letter != char(std::tolower(key[0])))
            --root;
          root += step % 7;
        }
        for (unsigned i = 0; i < tones; ++i)
          add(root + 2 * i);
        if (t.pitches.size() != tones)
          continue;
        int third = t.pitches[1].midi - t.pitches[0].midi,
            fifth = t.pitches[2].midi - t.pitches[0].midi;
        t.symbol = pitchName(t.pitches[0]) + (fifth == 6   ? "dim"
                                              : fifth == 8 ? "+"
                                              : third == 3 ? "m"
                                                           : "");
        if (tones == 4) {
          int seventh = t.pitches[3].midi - t.pitches[0].midi;
          if (fifth == 6 && seventh == 10)
            t.symbol = pitchName(t.pitches[0]) + "m7b5";
          else
            t.symbol += seventh == 11 ? "maj7" : "7";
        }
        if (o["inversions"] == true) {
          unsigned inv = random(tones);
          for (unsigned i = 0; i < inv; ++i) {
            t.pitches[i].midi += 12;
            t.pitches[i].octave += 1;
          }
        }
        if (g == "sevenths" && o["openVoicing"] == true) {
          std::sort(t.pitches.begin(), t.pitches.end(),
                    [](auto a, auto b) { return a.midi < b.midi; });
          t.pitches[1].midi += 12;
          t.pitches[1].octave += 1;
        }
      }
      std::sort(t.pitches.begin(), t.pitches.end(),
                [](auto a, auto b) { return a.midi < b.midi; });
      if (t.pitches.empty())
        continue;
      bool valid = true;
      for (unsigned i = 0; i < t.pitches.size(); ++i)
        if (t.pitches[i].midi < low || t.pitches[i].midi > high ||
            (i && t.pitches[i].midi == t.pitches[i - 1].midi))
          valid = false;
      unsigned split = hands == 1 ? unsigned(t.pitches.size())
                                  : (unsigned(t.pitches.size()) + 1) / 2;
      if (t.pitches[split - 1].midi - t.pitches[0].midi > 12)
        valid = false;
      if (split < t.pitches.size() &&
          t.pitches.back().midi - t.pitches[split].midi > 12)
        valid = false;
      if (valid && !out.empty() && smooth) {
        double old = 0, now = 0;
        for (auto p : out.back().pitches)
          old += p.midi;
        for (auto p : t.pitches)
          now += p.midi;
        if (std::abs(now / t.pitches.size() - old / out.back().pitches.size()) >
            14 - 2 * smooth)
          valid = false;
      }
      if (valid && g == "chordSymbol" && !out.empty() &&
          o["commonNotes"].get<int>()) {
        auto a = classes(t, o["ignoreAbove"]),
             b = classes(out.back(), o["ignoreAbove"]);
        unsigned common = 0;
        for (int pc : a)
          common += b.count(pc);
        if (common < unsigned(o["commonNotes"].get<int>()))
          valid = false;
      }
      if (valid) {
        out.push_back(std::move(t));
        found = true;
      }
    }
    if (!found)
      throw std::invalid_argument(
          "No compatible target after 4096 candidates. Widen range, reduce "
          "smoothness/common notes, allow inversions or use two hands.");
  }
  return out;
}
json Practice::configure(const json &patch) {
  auto c = validatePracticeConfig(merged(config_, patch));
  if (patch.size() == 1 && patch.contains("hints")) {
    config_ = std::move(c);
    ++revision_;
    return config_;
  }
  generatePractice(c, c["seed"], 64);
  config_ = c;
  running_ = false;
  paused_ = false;
  index_ = hits_ = misses_ = 0;
  origin_ = stoppedFrame_ = 0;
  error_.clear();
  targets_.clear();
  results_.clear();
  attempt_.clear();
  ++revision_;
  return config_;
}
void Practice::start(uint64_t frame) {
  auto t = generatePractice(config_, config_["seed"], 64);
  targets_ = std::move(t);
  results_.assign(targets_.size(), "target");
  index_ = hits_ = misses_ = 0;
  origin_ = stoppedFrame_ = frame;
  scoringFrom_ = frame;
  running_ = true;
  paused_ = false;
  error_.clear();
  attempt_.clear();
  ++revision_;
}
void Practice::stop(uint64_t frame) {
  if (!running_ && !paused_)
    return;
  if (running_) stoppedFrame_ = frame;
  running_ = false;
  paused_ = false;
  attempt_.clear();
  ++revision_;
}
void Practice::pause(uint64_t frame) {
  if (!running_) return;
  running_ = false;
  paused_ = true;
  stoppedFrame_ = frame;
  attempt_.clear();
  ++revision_;
}
void Practice::resume(uint64_t frame) {
  if (!paused_) return;
  if (frame >= stoppedFrame_) origin_ += frame - stoppedFrame_;
  stoppedFrame_ = frame;
  paused_ = false;
  running_ = true;
  scoringFrom_ = frame;
  attempt_.clear();
  ++revision_;
}
void Practice::reset(uint64_t frame) { start(frame); }
void Practice::miss() {
  ++misses_;
  attempt_.clear();
  ++revision_;
}
void Practice::hit(uint64_t frame) {
  results_[index_] = "hit";
  ++hits_;
  ++index_;
  attempt_.clear();
  ++revision_;
  if (index_ == targets_.size()) {
    running_ = false;
    stoppedFrame_ = frame;
  }
}
void Practice::evaluate(uint64_t frame) {
  if (!running_ || index_ >= targets_.size() || attempt_.empty())
    return;
  auto &t = targets_[index_];
  if (attempt_.size() != t.pitches.size())
    return;
  std::set<int> held;
  for (auto [o, n] : pressed_)
    held.insert(n);
  if (held.size() != t.pitches.size())
    return;
  uint64_t earliest = frame;
  for (auto p : t.pitches)
    if (!held.count(p.midi) || !attempt_.count(p.midi))
      return;
  for (auto [n, f] : attempt_)
    earliest = std::min(earliest, f);
  if (frame - earliest > uint64_t(rate_) * 150 / 1000) {
    miss();
    return;
  }
  hit(frame);
}
void Practice::input(const InputEvent &e) {
  auto owner =
      std::tuple<unsigned, unsigned, uint64_t>{e.source, e.channel, e.instance};
  if (e.type == EventType::Panic || e.type == EventType::ResetLive) {
    pressed_.clear();
    attempt_.clear();
    return;
  }
  if (e.type == EventType::Disconnect) {
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
  if (e.type == EventType::StopBus) {
    pressed_.clear();
    attempt_.clear();
    return;
  }
  if (e.type == EventType::Off) {
    auto i = pressed_.find(owner);
    if (i != pressed_.end()) {
      int n = i->second;
      pressed_.erase(i);
      bool other = false;
      for (auto [o, p] : pressed_)
        other |= p == n;
      if (!other)
        attempt_.erase(n);
    }
    return;
  }
  if (e.type != EventType::On)
    return;
  if (pressed_.contains(owner))
    return;
  pressed_[owner] = e.note;
  if (!running_ || e.frame < scoringFrom_ || index_ >= targets_.size())
    return;
  if (config_["mode"] == "scroll") {
    tick(e.frame);
    if (!running_)
      return;
    uint64_t due = origin_ + uint64_t(index_) * rate_ * 60 /
                                 config_["tempo"].get<unsigned>(),
             tol = uint64_t(rate_) * config_["toleranceMs"].get<unsigned>() /
                   1000;
    if (e.frame + tol < due) {
      miss();
      return;
    }
  }
  bool expected = false;
  for (auto p : targets_[index_].pitches)
    expected |= p.midi == e.note;
  if (!expected) {
    miss();
    return;
  }
  if (!attempt_.empty()) {
    uint64_t first = e.frame;
    for (auto [n, f] : attempt_)
      first = std::min(first, f);
    if (e.frame - first > uint64_t(rate_) * 150 / 1000)
      miss();
  }
  attempt_[e.note] = e.frame;
  evaluate(e.frame);
}
void Practice::tick(uint64_t frame) {
  if (!running_)
    return;
  if (!attempt_.empty()) {
    uint64_t first = frame;
    for (auto [n, f] : attempt_)
      first = std::min(first, f);
    if (frame > first + uint64_t(rate_) * 150 / 1000) {
      if (config_["mode"] == "wait")
        miss();
      else
        attempt_
            .clear(); // Scroll counts this incomplete target at its deadline.
    }
  }
  if (config_["mode"] != "scroll")
    return;
  uint64_t tol =
      uint64_t(rate_) * config_["toleranceMs"].get<unsigned>() / 1000;
  while (running_ && frame > origin_ + targetFrame(index_) + tol) {
    results_[index_] = "miss";
    miss();
    ++index_;
    if (index_ == targets_.size()) {
      running_ = false;
      stoppedFrame_ = frame;
    }
  }
}
json Practice::snapshot(uint64_t frame) const {
  json targets = json::array(), notes = json::array(), held = json::array(),
       hints = json::array();
  for (unsigned i = 0; i < targets_.size(); ++i) {
    auto t = targets_[i].jsonValue();
    t["index"] = i;
    t["result"] = results_[i];
    t["startFrame"] = targetFrame(i);
    t["endFrame"] = targetFrame(i + 1);
    targets.push_back(t);
    unsigned split = (targets_[i].pitches.size() + 1) / 2;
    for (unsigned j = 0; j < targets_[i].pitches.size(); ++j) {
      auto p = targets_[i].pitches[j];
      std::string staff =
          config_["staff"] == "bass" ? "bass"
          : config_["staff"] == "grand"
              ? (config_["hand"] == "left" ? "bass"
                 : config_["hand"] == "right" ? "treble"
                 : j < split && targets_[i].pitches.size() > 1 ? "bass"
                 : p.midi < 60                               ? "bass"
                                                             : "treble")
              : "treble";
      if (config_["hints"] == true && i == index_)
        hints.push_back({{"midiNote", p.midi},
                         {"hand", staff == "bass" ? "left" : "right"}});
      notes.push_back({{"id", std::to_string(i) + ":" + std::to_string(j)},
                       {"midiNote", p.midi},
                       {"spelling", p.notation()},
                       {"startFrame", targetFrame(i)},
                       {"endFrame", targetFrame(i + 1)},
                       {"staff", staff},
                       {"duration", "q"},
                       {"highlight", results_[i]}});
    }
  }
  for (auto [o, n] : pressed_)
    held.push_back({{"sourceId", std::get<0>(o)},
                    {"channel", std::get<1>(o)},
                    {"instance", std::get<2>(o)},
                    {"note", n}});
  uint64_t current = running_ ? frame : stoppedFrame_;
  double elapsed = current >= origin_ ? double(current - origin_) / rate_ : 0;
  return {{"config", config_},
          {"running", running_},
          {"paused", paused_},
          {"revision", revision_},
          {"originFrame", origin_},
          {"clockFrame", config_["mode"] == "wait" ? targetFrame(index_)
                         : current >= origin_      ? current - origin_
                                                   : 0},
          {"index", index_},
          {"hits", hits_},
          {"misses", misses_},
          {"accuracy", hits_ + misses_ ? 100.0 * hits_ / (hits_ + misses_) : 0},
          {"elapsed", elapsed},
          {"error", error_},
          {"targets", targets},
          {"notes", notes},
          {"hintKeys", hints},
          {"pressed", held}};
}
} // namespace sightreading
uint64_t sightreading::Practice::targetFrame(unsigned index) const {
  return uint64_t(index) * rate_ * 60 / config_["tempo"].get<unsigned>();
}
void sightreading::Practice::recoverHeld(const json &notes, uint64_t frame) {
  stop(frame);
  pressed_.clear();
  attempt_.clear();
  for (const auto &n : notes)
    if (n["bus"] == "live" && n["held"] == true)
      pressed_[{n["sourceId"].get<unsigned>(), n["channel"].get<unsigned>(),
                n["instance"].get<uint64_t>()}] = n["note"];
  error_ = "Input events were lost. Exercise stopped; restart after reducing "
           "MIDI traffic.";
  ++revision_;
}
