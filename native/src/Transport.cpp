#include <algorithm>
#include <cmath>
#include <numeric>
#include <sightreading/Practice.hpp>
#include <sightreading/Transport.hpp>
namespace sightreading {
using json = nlohmann::json;
json inferChords(const json &s) {
  if (s.contains("chords"))
    return s["chords"];
  json result = json::array();
  double end = s["durationTicks"], ppq = s["ppq"], at = 0;
  std::vector<std::vector<unsigned>> templates = {
      {0, 4, 7},     {0, 3, 7},     {0, 3, 6},   {0, 4, 7, 10},
      {0, 4, 7, 11}, {0, 3, 7, 10}, {0, 3, 6, 9}};
  while (at < end) {
    unsigned numerator = 4, denominator = 4;
    double nextMeter = end;
    for (auto &m : s["meterMap"]) {
      if (m["tick"].get<double>() <= at) {
        numerator = m["numerator"];
        denominator = m["denominator"];
      } else {
        nextMeter = m["tick"];
        break;
      }
    }
    double to =
        std::min({end, at + ppq * 4 * numerator / denominator, nextMeter});
    std::array<double, 12> weights{};
    double total = 0;
    for (auto &n : s["notes"]) {
      if (n["channel"] == 9)
        continue;
      double overlap =
          std::max(0.0, std::min(to, n["endTick"].get<double>()) -
                            std::max(at, n["startTick"].get<double>()));
      weights[n["midiNote"].get<unsigned>() % 12] += overlap;
      total += overlap;
    }
    double best = 0;
    unsigned root = 0, type = 0;
    for (unsigned r = 0; r < 12; r++)
      for (unsigned q = 0; q < templates.size(); q++) {
        double covered = 0;
        for (auto pc : templates[q])
          covered += weights[(r + pc) % 12];
        if (covered > best) {
          best = covered;
          root = r;
          type = q;
        }
      }
    double confidence = total ? best / total : 0;
    json pitches = json::array();
    if (confidence >= .7)
      for (auto pc : templates[type])
        pitches.push_back(48 + root + pc);
    result.push_back({{"startTick", at},
                      {"endTick", to},
                      {"root", root},
                      {"quality", type},
                      {"confidence", confidence},
                      {"pitches", pitches}});
    at = to;
  }
  return result;
}
bool Transport::audible(unsigned t) const {
  auto &track = song_["tracks"][t];
  return !track.value("percussion", false) &&
         (track["backing"].get<bool>() ||
          (track["practice"].get<bool>() &&
           config_["hearPractice"].get<bool>()));
}
bool Transport::inSelection(const Group &group) const {
  return config_["loop"] != true ||
         (group.tick >= config_["a"].get<double>() && group.tick < config_["b"].get<double>());
}
double Transport::frameSeconds(uint64_t frame) const {
  return cycleStart_ + (double(frame) - double(origin_)) *
                           config_["speed"].get<double>() /
                           engine_.sampleRate();
}
double Transport::position(uint64_t frame) const {
  if (returning_) {
    const double t = std::clamp((double(frame)-double(returnStart_))/(engine_.sampleRate()*2.0),0.0,1.0);
    return returnFrom_+(returnTo_-returnFrom_)*t*t*(3-2*t);
  }
  if (!playing_)
    return position_;
  double p = clockStart_ + std::max(0.0, double(frame) - double(clockOrigin_)) *
                               config_["speed"].get<double>() /
                               engine_.sampleRate();
  if (config_["loop"].get<bool>() && p >= cycleEnd_) {
    p = cycleEnd_;
  }
  return std::clamp(p, 0.0, song_.value("durationSeconds", 0.0));
}
void Transport::cancel() {
  engine_.submit({EventType::StopBus, Bus::Playback});
  playing_ = false;
  returning_ = returnPlay_ = false;
  ++generation_;
  attempt_.clear();
  active_.clear();
}
void Transport::load(json s) {
  const bool samePiece = !song_.is_null() && song_["id"] == s["id"] &&
                         song_["durationTicks"] == s["durationTicks"];
  cancel();
  song_ = std::move(s);
  window_ = std::make_unique<SongWindow>(song_);
  prepareSongTiming(song_);
  position_ = 0;
  if (!samePiece) {
    config_["a"] = 0;
    config_["b"] = song_["durationTicks"];
  }
  retry_ = false;
  hits_ = misses_ = 0;
  groupFailed_ = false;
  build();
  group_ = 0;
  ++revision_;
}
void Transport::build() {
  timeline_.clear();
  groups_.clear();
  hintGroups_.clear();
  inferenceNotes_.clear();
  cachedChord_ = nullptr;
  for (auto &channel : controls_)
    for (auto &values : channel)
      values.clear();
  std::map<double, std::set<unsigned>> grouped;
  std::map<double, std::map<unsigned, unsigned>> hands;
  std::map<double, std::set<unsigned>> hintPitches;
  std::map<double, std::map<unsigned, unsigned>> hintHands;
  for (auto &n : song_["notes"]) {
    if (n["channel"] != 9)
      inferenceNotes_.push_back({n["startTick"].get<double>(),
                                 n["endTick"].get<double>(),
                                 n["midiNote"].get<unsigned>() % 12});
    auto track = n["trackId"].get<unsigned>(),
         pitch = n["midiNote"].get<unsigned>();
    double from = n["startTick"], to = n["endTick"];
    const auto staff = song_["tracks"][track]["staff"];
    const bool left = staff == "bass" || (staff != "treble" && pitch < 60);
    const bool selected = config_["hand"] == "both" ||
                          (config_["hand"] == "left" ? left : !left);
    const bool practice = song_["tracks"][track]["practice"] == true;
    const bool enabled = practice || song_["tracks"][track]["backing"] == true;
    const bool pitched = n["channel"] != 9 &&
                         !song_["tracks"][track].value("percussion", false) &&
                         pitch >= 21 && pitch <= 108;
    if (selected && enabled && pitched) {
      hintPitches[from].insert(pitch);
      hintHands[from][pitch] |= left ? 1 : 2;
    }
    if (selected && pitched && (practice || (config_["mode"] == "wait" && enabled))) {
      grouped[from].insert(pitch);
      hands[from][pitch] |= left ? 1 : 2;
    }
    if (!audible(track) || pitch < 21 || pitch > 108)
      continue;
    InputEvent on{EventType::On,
                  Bus::Playback,
                  SongSource,
                  uint8_t(n["channel"].get<unsigned>()),
                  uint8_t(pitch),
                  uint8_t(n["velocity"].get<unsigned>()),
                  n["id"].get<uint64_t>()};
    auto off = on;
    off.type = EventType::Off;
    timeline_.push_back({tickSeconds(song_, from), on, track,
                         n.value("startOrder", n["id"].get<uint64_t>() * 2)});
    timeline_.push_back({tickSeconds(song_, to), off, track,
                         n.value("endOrder", n["id"].get<uint64_t>() * 2 + 1)});
  }
  for (auto &[tick, pitches] : grouped)
    groups_.push_back({tickSeconds(song_, tick), tick, pitches, hands[tick]});
  for (auto &[tick, pitches] : hintPitches)
    hintGroups_.push_back({tickSeconds(song_, tick), tick, pitches, hintHands[tick]});
  for (auto &c : song_["controllers"]) {
    if (!audible(c["trackId"]))
      continue;
    unsigned cc = c["controller"];
    auto &values = controls_[c["channel"].get<unsigned>()][cc];
    int lastUp = values.empty() ? -1 : values.back().lastUp;
    if (c["value"].get<unsigned>() < 64)
      lastUp = int(values.size());
    values.push_back({c["tick"], c["value"], lastUp});
    timeline_.push_back(
        {tickSeconds(song_, c["tick"]),
         {cc == 64 ? EventType::Pedal : EventType::Controller, Bus::Playback,
          SongSource, uint8_t(c["channel"].get<unsigned>()), uint8_t(cc),
          uint8_t(c["value"].get<unsigned>())},
         c["trackId"].get<unsigned>(),
         c.value("order", uint64_t{})});
  }
  // Point-query index includes pedal-extended tails and channel kill
  // boundaries.
  restoreNotes_.clear();
  std::array<std::vector<double>, 16> ups;
  for (unsigned ch = 0; ch < 16; ++ch)
    for (const auto &c : controls_[ch][64])
      if (c.value < 64)
        ups[ch].push_back(c.tick);
  for (size_t i = 0; i < song_["notes"].size(); ++i) {
    const auto &n = song_["notes"][i];
    if (!audible(n["trackId"]) || n["midiNote"] < 21 || n["midiNote"] > 108)
      continue;
    double start = n["startTick"], end = n["endTick"];
    unsigned ch = n["channel"];
    const auto &pedals = controls_[ch][64];
    auto at =
        std::upper_bound(pedals.begin(), pedals.end(), end,
                         [](double t, const Control &c) { return t < c.tick; });
    if (at != pedals.begin() && std::prev(at)->value >= 64) {
      auto up = std::upper_bound(ups[ch].begin(), ups[ch].end(), end);
      end = up == ups[ch].end() ? song_["durationTicks"].get<double>() : *up;
    }
    for (unsigned cc : {120u, 123u}) {
      const auto &v = controls_[ch][cc];
      auto kill = std::upper_bound(
          v.begin(), v.end(), start,
          [](double t, const Control &c) { return t < c.tick; });
      if (kill != v.end())
        end = std::min(end, kill->tick);
    }
    restoreNotes_.push_back({start, end, i});
  }
  std::sort(restoreNotes_.begin(), restoreNotes_.end(),
            [](const auto &a, const auto &b) { return a.start < b.start; });
  restoreBase_ = 1;
  while (restoreBase_ < restoreNotes_.size())
    restoreBase_ *= 2;
  restoreEnds_.assign(restoreBase_ * 2, -1);
  for (size_t i = 0; i < restoreNotes_.size(); ++i)
    restoreEnds_[restoreBase_ + i] = restoreNotes_[i].end;
  for (size_t i = restoreBase_ - 1; i > 0; --i)
    restoreEnds_[i] = std::max(restoreEnds_[i * 2], restoreEnds_[i * 2 + 1]);
  std::stable_sort(timeline_.begin(), timeline_.end(), [](auto &a, auto &b) {
    if (a.seconds != b.seconds)
      return a.seconds < b.seconds;
    return a.order < b.order;
  });
  std::sort(inferenceNotes_.begin(), inferenceNotes_.end(),
            [](auto &a, auto &b) { return a.start < b.start; });
  inferenceBase_ = 1;
  while (inferenceBase_ < inferenceNotes_.size())
    inferenceBase_ *= 2;
  inferenceEnds_.assign(inferenceBase_ * 2, -1);
  for (size_t i = 0; i < inferenceNotes_.size(); ++i)
    inferenceEnds_[inferenceBase_ + i] = inferenceNotes_[i].end;
  for (size_t i = inferenceBase_ - 1; i > 0; --i)
    inferenceEnds_[i] =
        std::max(inferenceEnds_[i * 2], inferenceEnds_[i * 2 + 1]);
  for (auto field : {"tempoMap", "meterMap"})
    for (auto &entry : song_[field])
      timeline_.push_back(
          {tickSeconds(song_, entry["tick"]), {EventType::Controller}, 257});
  std::stable_sort(timeline_.begin(), timeline_.end(), [](auto &a, auto &b) {
    if (a.seconds != b.seconds)
      return a.seconds < b.seconds;
    return a.order < b.order;
  });
}
void Transport::follow(double seconds, uint64_t frame) {
  double tick = secondsTick(song_, seconds), bpm = 120, meterTick = 0;
  unsigned num = 4, den = 4;
  auto &tempos = song_["tempoMap"];
  auto tempo = std::upper_bound(
      tempos.begin(), tempos.end(), tick,
      [](double t, auto &e) { return t < e["tick"].template get<double>(); });
  if (tempo != tempos.begin())
    --tempo;
  bpm = 60000000.0 / (*tempo)["microseconds"].get<double>() *
        config_["speed"].get<double>();
  auto &meters = song_["meterMap"];
  auto meter = std::upper_bound(
      meters.begin(), meters.end(), tick,
      [](double t, auto &e) { return t < e["tick"].template get<double>(); });
  if (meter != meters.begin())
    --meter;
  num = (*meter)["numerator"];
  den = (*meter)["denominator"];
  meterTick = (*meter)["tick"];
  if (bpm < 20 || bpm > 300 || num > 12 ||
      (den != 2 && den != 4 && den != 8 && den != 16)) {
    followError_ = "This tempo/meter map is outside metronome Follow's 20–300 "
                   "BPM, 1–12 beats, 2/4/8/16 denominator range. Clicks "
                   "continue at their last supported setting.";
    return;
  }
  double unit = song_["ppq"].get<double>() * 4 / den,
         nextBeat = std::ceil((tick - meterTick) / unit - 1e-9),
         boundary = meterTick + nextBeat * unit;
  uint64_t target =
      frame + uint64_t(std::llround(
                  std::max(0.0, tickSeconds(song_, boundary) - seconds) *
                  engine_.sampleRate() / config_["speed"].get<double>()));
  try {
    engine_.followMetronome(bpm, num, den, unsigned(nextBeat) % num, target);
    followError_.clear();
  } catch (const std::exception &e) {
    followError_ = std::string("Metronome Follow capacity: ") + e.what();
  }
}
bool Transport::weights(size_t node, size_t left, size_t right, double start,
                        double end, std::array<double, 12> &values,
                        unsigned &count) const {
  if (left >= inferenceNotes_.size() || inferenceNotes_[left].start >= end ||
      inferenceEnds_[node] <= start)
    return true;
  if (right - left == 1) {
    auto &n = inferenceNotes_[left];
    double overlap =
        std::max(0.0, std::min(end, n.end) - std::max(start, n.start));
    if (overlap) {
      if (++count > 16384)
        return false;
      values[n.pc] += overlap;
    }
    return true;
  }
  size_t middle = (left + right) / 2;
  return weights(node * 2, left, middle, start, end, values, count) &&
         weights(node * 2 + 1, middle, right, start, end, values, count);
}
json Transport::barChord(double tick, double &start, double &end) {
  if (!cachedChord_.is_null() &&
      cachedChord_["startTick"].get<double>() <= tick &&
      cachedChord_["endTick"].get<double>() > tick) {
    start = cachedChord_["startTick"];
    end = cachedChord_["endTick"];
    return cachedChord_;
  }
  auto &meters = song_["meterMap"];
  auto it = std::upper_bound(
      meters.begin(), meters.end(), tick,
      [](double t, auto &e) { return t < e["tick"].template get<double>(); });
  double next = it == meters.end() ? song_["durationTicks"].get<double>()
                                   : (*it)["tick"].get<double>();
  if (it != meters.begin())
    --it;
  double length = song_["ppq"].get<double>() * 4 *
                  (*it)["numerator"].get<double>() /
                  (*it)["denominator"].get<double>(),
         base = (*it)["tick"];
  start = base + std::floor((tick - base) / length) * length;
  end = std::min(start + length, next);
  if (song_.contains("chords")) {
    for (auto &chord : song_["chords"])
      if (chord["startTick"].get<double>() <= tick &&
          chord["endTick"].get<double>() > tick) {
        start = chord["startTick"];
        end = chord["endTick"];
        cachedChord_ = chord;
        return chord;
      }
    return {{"pitches", json::array()}};
  }
  std::array<double, 12> values{};
  unsigned count = 0;
  if (!weights(1, 0, inferenceBase_, start, end, values, count))
    throw std::runtime_error(
        "Accompaniment inference density exceeds its bounded 16384 "
        "overlapping-note window; original playback continues.");
  double total = 0;
  for (double v : values)
    total += v;
  const std::vector<std::vector<unsigned>> templates = {
      {0, 4, 7},     {0, 3, 7},     {0, 3, 6},   {0, 4, 7, 10},
      {0, 4, 7, 11}, {0, 3, 7, 10}, {0, 3, 6, 9}};
  double best = 0;
  unsigned root = 0, type = 0;
  for (unsigned r = 0; r < 12; r++)
    for (unsigned q = 0; q < templates.size(); q++) {
      double covered = 0;
      for (unsigned pc : templates[q])
        covered += values[(r + pc) % 12];
      if (covered > best) {
        best = covered;
        root = r;
        type = q;
      }
    }
  double confidence = total ? best / total : 0;
  json pitches = json::array();
  if (confidence >= .7)
    for (unsigned pc : templates[type])
      pitches.push_back(48 + root + pc);
  cachedChord_ = {{"startTick", start},
                  {"endTick", end},
                  {"root", root},
                  {"confidence", confidence},
                  {"pitches", pitches}};
  return cachedChord_;
}
void Transport::accompany(uint64_t frame, double until) {
  if (config_["accompaniment"] == "Off" || !accompanimentError_.empty())
    return;
  const auto style = config_["accompaniment"].get<std::string>();
  double ppq = song_["ppq"], speed = config_["speed"],
         step = ppq / config_["rate"].get<double>() /
                (style == "Bossa Nova" ? 2 : 1),
         stop = secondsTick(song_, until);
  unsigned work = 0;
  try {
    while (nextAccompanimentTick_ < stop &&
           nextAccompanimentTick_ < song_["durationTicks"].get<double>()) {
      double start, end;
      auto chord = barChord(nextAccompanimentTick_, start, end);
      double tick =
          start +
          std::ceil((nextAccompanimentTick_ - start) / step - 1e-9) * step;
      if (tick >= end) {
        nextAccompanimentTick_ = end;
        continue;
      }
      double onSeconds = tickSeconds(song_, tick),
             offSeconds =
                 tickSeconds(song_, std::min({tick + step * .8, end,
                                              secondsTick(song_, cycleEnd_)}));
      if (onSeconds >= until)
        break;
      uint64_t onFrame = origin_ +
                         uint64_t(std::llround((onSeconds - cycleStart_) *
                                               engine_.sampleRate() / speed)),
               offFrame = origin_ +
                          uint64_t(std::llround((offSeconds - cycleStart_) *
                                                engine_.sampleRate() / speed));
      if (offFrame <= onFrame || ++work > 128)
        throw std::runtime_error("Accompaniment density exceeds sample-clock "
                                 "capacity; original playback continues.");
      auto pitches = chord["pitches"].get<std::vector<unsigned>>();
      if (pitches.empty() && chord.value("confidence", 0.0) == 0) {
        auto next = std::lower_bound(
            inferenceNotes_.begin(), inferenceNotes_.end(), end,
            [](auto &n, double t) { return n.start < t; });
        nextAccompanimentTick_ = next == inferenceNotes_.end()
                                     ? song_["durationTicks"].get<double>()
                                     : next->start;
        continue;
      }
      if (!pitches.empty()) {
        unsigned beat = unsigned(std::llround((tick - start) / step));
        std::vector<unsigned> use;
        if (style == "Root")
          use = {pitches[0] - 12};
        else if (style == "Root+5")
          use = {pitches[0] - 12, pitches[0] - 5};
        else if (style == "Arp")
          use = {pitches[beat % pitches.size()]};
        else if (style == "Bossa Nova") {
          unsigned pattern = beat % 8;
          if (pattern == 0 || pattern == 4)
            use = {pitches[0] - 12};
          else if (pattern == 1 || pattern == 3 || pattern == 6)
            use = pitches;
        } else
          use = std::vector<unsigned>(pitches.begin(),
                                      pitches.begin() +
                                          std::min<size_t>(3, pitches.size()));
        for (unsigned pitch : use) {
          if (config_["accompanimentSpacing"].get<double>() > 1 &&
              pitch != use[0])
            pitch += 12;
          InputEvent on{EventType::On,
                        Bus::Playback,
                        AccompanimentSource,
                        0,
                        uint8_t(pitch),
                        65,
                        ++accompanimentInstance_,
                        onFrame};
          auto off = on;
          off.type = EventType::Off;
          off.frame = offFrame;
          if (!engine_.submit(on) || !engine_.submit(off))
            throw std::runtime_error(
                "Accompaniment queue pressure; original playback continues.");
        }
      }
      nextAccompanimentTick_ = tick + step;
    }
  } catch (const std::exception &e) {
    accompanimentError_ = e.what();
    engine_.submit({EventType::Disconnect, Bus::Playback, AccompanimentSource});
  }
}
bool Transport::restore(double seconds, uint64_t frame, unsigned budget) {
  std::vector<InputEvent> pending;
  std::map<std::pair<unsigned, uint64_t>, InputEvent> restored;
  auto reject = [&]() {
    error_ = "Playback restoration capacity exceeded; reduce active tracks or "
             "choose a less dense seek position.";
    engine_.submit({EventType::Disconnect, Bus::Playback, SongSource});
    active_.clear();
    playing_ = false;
    retry_ = false;
    return false;
  };

  auto stage = [&](InputEvent e) { pending.push_back(e); };
  const double seekTick = secondsTick(song_, seconds);
  auto latest = [&](unsigned channel, unsigned cc,
                    double tick) -> const Control * {
    const auto &v = controls_[channel][cc];
    auto it = std::upper_bound(
        v.begin(), v.end(), tick,
        [](double at, const Control &c) { return at < c.tick; });
    return it == v.begin() ? nullptr : &*std::prev(it);
  };
  std::map<std::pair<unsigned, unsigned>, unsigned> controls;
  for (unsigned channel = 0; channel < 16; ++channel)
    for (unsigned cc = 0; cc < 128; ++cc)
      if (auto c = latest(channel, cc, seekTick))
        controls[{channel, cc}] = c->value;
  for (unsigned channel = 0; channel < 16; ++channel) {
    stage({EventType::Controller, Bus::Playback, SongSource, uint8_t(channel),
           7, 100, 0, frame});
    stage({EventType::Controller, Bus::Playback, SongSource, uint8_t(channel),
           11, 127, 0, frame});
    stage({EventType::Controller, Bus::Playback, SongSource, uint8_t(channel),
           10, 64, 0, frame});
  }
  for (auto &[key, value] : controls)
    if (key.second != 120 && key.second != 123)
      stage({key.second == 64 ? EventType::Pedal : EventType::Controller,
             Bus::Playback, SongSource, uint8_t(key.first), uint8_t(key.second),
             uint8_t(value), 0, frame});
  std::vector<size_t> candidates;
  if (seconds > 0) {
    struct Node {
      size_t node, left, right;
    };
    std::vector<Node> stack{{1, 0, restoreBase_}};
    unsigned inspected = 0;
    while (!stack.empty()) {
      auto q = stack.back();
      stack.pop_back();
      if (q.left >= restoreNotes_.size() ||
          restoreNotes_[q.left].start >= seekTick ||
          restoreEnds_[q.node] <= seekTick)
        continue;
      if (++inspected > 65536)
        return reject();
      if (q.right - q.left == 1) {
        candidates.push_back(restoreNotes_[q.left].index);
        if (candidates.size() > 1024)
          return reject();
      } else {
        auto middle = (q.left + q.right) / 2;
        stack.push_back({q.node * 2 + 1, middle, q.right});
        stack.push_back({q.node * 2, q.left, middle});
      }
    }
  }
  for (auto index : candidates) {
    const auto &n = song_["notes"][index];
    if (!audible(n["trackId"]) || n["midiNote"].get<unsigned>() < 21 ||
        n["midiNote"].get<unsigned>() > 108)
      continue;
    double start = tickSeconds(song_, n["startTick"]),
           end = tickSeconds(song_, n["endTick"]);
    bool sustained = false;
    if (start < seconds && end <= seconds) {
      unsigned ch = n["channel"];
      auto atEnd = latest(ch, 64, n["endTick"]);
      auto atSeek = latest(ch, 64, seekTick);
      const auto &pedals = controls_[ch][64];
      sustained = atEnd && atEnd->value >= 64 && atSeek &&
                  (atSeek->lastUp < 0 || pedals[size_t(atSeek->lastUp)].tick <=
                                             n["endTick"].get<double>());
    }
    bool killed = false;
    for (unsigned cc : {120u, 123u})
      if (auto c = latest(n["channel"], cc, seekTick))
        killed |= c->tick > n["startTick"].get<double>();
    if (!killed && start < seconds && (end > seconds || sustained)) {
      InputEvent on{EventType::On,
                    Bus::Playback,
                    SongSource,
                    uint8_t(n["channel"].get<unsigned>()),
                    uint8_t(n["midiNote"].get<unsigned>()),
                    uint8_t(n["velocity"].get<unsigned>()),
                    n["id"].get<uint64_t>(),
                    frame};
      stage(on);
      restored[{on.source, on.instance}] = on;
      if (restored.size() > 1024 || pending.size() > budget)
        return reject();
      if (sustained) {
        on.type = EventType::Off;
        stage(on);
      }
    }
  }
  if (pending.size() > budget)
    return reject();
  for (const auto &e : pending)
    if (!engine_.submit(e))
      return reject();
  active_ = std::move(restored);
  return true;
}
void Transport::resume(uint64_t frame) {
  origin_ = frame + 256;
  cycleStart_ = position_;
  cycleEnd_ = config_["loop"] == true ? tickSeconds(song_, config_["b"])
                                      : song_["durationSeconds"].get<double>();
  if (cycleStart_ >= cycleEnd_ ||
      (config_["loop"] == true && cycleStart_ < tickSeconds(song_, config_["a"])))
    cycleStart_ = position_ =
        config_["loop"] == true ? tickSeconds(song_, config_["a"]) : 0;
  clockOrigin_ = origin_;
  clockStart_ = cycleStart_;
  lastScorePosition_ = cycleStart_;
  nextAccompanimentTick_ = secondsTick(song_, cycleStart_);
  accompanimentError_.clear();
  cursor_ = std::lower_bound(timeline_.begin(), timeline_.end(), cycleStart_,
                             [](auto &e, double v) { return e.seconds < v; }) -
            timeline_.begin();
  const auto previousGroup = group_;
  const bool frozenRetry = retry_;
  group_ = std::lower_bound(groups_.begin(), groups_.end(), cycleStart_,
                            [](auto &g, double v) { return g.seconds < v; }) -
           groups_.begin();
  if (!restore(cycleStart_, origin_))
    throw std::runtime_error(error_);
  if (!frozenRetry || previousGroup != group_)
    groupFailed_ = false;
  attempt_.clear();
  playing_ = true;
  retry_ = false;
  error_.clear();
  follow(cycleStart_, origin_);
}
json Transport::command(const std::string &type, const json &params,
                        uint64_t frame) {
  if (song_.is_null() && type != "transport.configure")
    throw std::invalid_argument("Choose a song first.");
  if (type == "transport.play") {
    if (returning_) returnPlay_ = true;
    else if (!playing_)
      resume(frame);
  } else if (type == "transport.return") {
    beginReturn(frame,position(frame),playing_ || returnPlay_ || (retry_ && config_["mode"]=="wait"));
  } else if (type == "transport.pause") {
    position_ = position(frame);
    cancel();
    retry_ = false;
  } else if (type == "transport.stop") {
    cancel();
    position_ = 0;
    retry_ = false;
    group_ = 0;
    groupFailed_ = false;
    attempt_.clear();
  } else if (type == "transport.restart") {
    cancel();
    position_ = config_["loop"] == true ? tickSeconds(song_, config_["a"]) : 0;
    retry_ = false;
    groupFailed_ = false;
    hits_ = misses_ = 0;
    resume(frame);
  } else if (type == "transport.seek") {
    double tick;
    if (params.contains("seconds")) {
      double seconds = params.at("seconds");
      if (!std::isfinite(seconds) || seconds < 0 ||
          seconds > song_["durationSeconds"].get<double>())
        throw std::invalid_argument("Seek is outside this song.");
      tick = secondsTick(song_, seconds);
    } else {
      tick = params.at("tick");
    }
    if (!std::isfinite(tick) || tick < 0 ||
        tick > song_["durationTicks"].get<double>())
      throw std::invalid_argument("Seek is outside this song.");
    bool running = playing_;
    cancel();
    position_ = tickSeconds(song_, tick);
    group_ = std::lower_bound(groups_.begin(), groups_.end(), position_,
                             [](const auto &g, double p) { return g.seconds < p; }) - groups_.begin();
    retry_ = false;
    groupFailed_ = false;
    attempt_.clear();
    if (running)
      resume(frame);
  } else if (type == "transport.tempo") {
    double bpm = params.at("bpm");
    if (!std::isfinite(bpm) || bpm < 20 || bpm > 300)
      throw std::invalid_argument("Tempo must be 20–300 BPM.");
    double tick = secondsTick(song_, position(frame)), micro = 500000;
    for (auto &entry : song_["tempoMap"])
      if (entry["tick"].get<double>() <= tick)
        micro = entry["microseconds"];
    return command("transport.configure", {{"speed", bpm * micro / 60000000.0}},
                   frame);
  } else if (type == "transport.configure") {
    auto next = config_;
    for (auto it = params.begin(); it != params.end(); ++it) {
      if (!next.contains(it.key()))
        throw std::invalid_argument("Unknown transport option.");
      next[it.key()] = it.value();
    }
    for (auto field : {"hearPractice", "practiceEnabled", "loop", "hints"})
      if (!next[field].is_boolean())
        throw std::invalid_argument("Transport switches must be boolean.");
    if (params.size() == 1 && params.contains("hints")) {
      config_["hints"] = next["hints"];
      return snapshot(frame);
    }
    auto number = [&](const char *key, double min, double max) {
      if (!next[key].is_number() || !std::isfinite(next[key].get<double>()) ||
          next[key].get<double>() < min || next[key].get<double>() > max)
        throw std::invalid_argument(std::string("Invalid ") + key);
    };
    number("speed", .000001, 1000000);
    number("toleranceMs", 20, 1000);
    number("spacing", 30, 300);
    number("rate", .25, 4);
    number("accompanimentSpacing", 1, 2);
    number("a", 0, song_.is_null() ? 0 : song_["durationTicks"].get<double>());
    number("b", 0, song_.is_null() ? 0 : song_["durationTicks"].get<double>());
    if (next["loop"] == true &&
        next["a"].get<double>() >= next["b"].get<double>())
      throw std::invalid_argument("Loop A must be before B.");
    if (next["mode"] != "continuous" && next["mode"] != "pauseOnMiss" && next["mode"] != "wait")
      throw std::invalid_argument("Unknown practice mode.");
    if (next["hand"] != "both" && next["hand"] != "left" && next["hand"] != "right")
      throw std::invalid_argument("Unknown practice hand.");
    std::set<std::string> styles = {"Off",    "Root", "Triad",
                                    "Root+5", "Arp",  "Bossa Nova"};
    if (!next["accompaniment"].is_string() ||
        !styles.contains(next["accompaniment"]))
      throw std::invalid_argument("Unknown accompaniment style.");
    const double currentPosition = song_.is_null() ? 0 : position(frame);
    if (params.contains("speed") && !song_.is_null()) {
      const double tick = secondsTick(song_, currentPosition);
      double micro = 500000;
      for (auto &entry : song_["tempoMap"])
        if (entry["tick"].get<double>() <= tick)
          micro = entry["microseconds"];
      const double bpm = 60000000.0 / micro * next["speed"].get<double>();
      if (bpm >= 20 && bpm <= 300) {
        const auto result = engine_.command({{"version", 1}, {"id", "transport-tempo"},
                                            {"type", "metronome.config"},
                                            {"params", {{"tempo", bpm}}}});
        if (result["ok"] != true)
          throw std::runtime_error("Metronome tempo update failed.");
      }
    }
    position_ = currentPosition;
    const bool wasReturning=returning_;
    bool running = playing_ || returnPlay_ || (retry_ && config_["mode"] == "wait");
    cancel();
    config_ = next;
    if (!song_.is_null()) build();
    if(wasReturning)beginReturn(frame,currentPosition,running);
    else if (running)
      resume(frame);
    ++revision_;
  } else
    throw std::invalid_argument("Unknown transport command.");
  return snapshot(frame);
}
void Transport::tick(uint64_t frame) {
  if (returning_) {
    if (double(frame)-double(returnStart_) < engine_.sampleRate()*2.0) return;
    const bool play=returnPlay_;
    position_=returnTo_; returning_=returnPlay_=false;
    group_=std::lower_bound(groups_.begin(),groups_.end(),position_,
      [](const auto& g,double p){return g.seconds<p;})-groups_.begin();
    if(play)try{resume(frame);}catch(const std::exception&){return;}
    return;
  }
  if (!playing_)
    return;
  double speed = config_["speed"], rate = engine_.sampleRate(),
         lookahead = .05 * speed;
  double now = frameSeconds(frame), until = now + lookahead;
  if (config_["mode"] == "wait") {
    if (group_ < groups_.size() && groups_[group_].seconds < cycleEnd_) {
      const double target = groups_[group_].seconds;
      if (position(frame) >= target) {
        position_ = target;
        cancel();
        retry_ = true;
        waitFrom_ = frame;
        return;
      }
      // Do not schedule target or later notes before the learner answers.
      until = std::min(until, target - 1e-9);
    }
  }
  unsigned submitted = 0;
  pumpEntries_ = 0;
  while (playing_) {
    while (cursor_ < timeline_.size() &&
           timeline_[cursor_].seconds < cycleEnd_ &&
           timeline_[cursor_].seconds <= until) {
      if (submitted >= 2048)
        return;
      ++submitted;
      pumpEntries_ = submitted;
      auto e = timeline_[cursor_++];
      e.input.frame =
          origin_ +
          uint64_t(std::llround((e.seconds - cycleStart_) * rate / speed));
      if (e.track == 257) {
        follow(e.seconds, e.input.frame);
        continue;
      }
      if (!engine_.submit(e.input)) {
        error_ = "Playback queue pressure; reduce accompaniment rate or song "
                 "density.";
        position_ = position(frame);
        cancel();
        return;
      }
      auto key =
          std::pair<unsigned, uint64_t>{e.input.source, e.input.instance};
      if (e.input.type == EventType::On)
        active_[key] = e.input;
      else if (e.input.type == EventType::Off)
        active_.erase(key);
    }
    accompany(frame, std::min(until, cycleEnd_));
    if (until < cycleEnd_)
      break;
    if (config_["loop"] != true) {
      if (now >= cycleEnd_) {
        position_ = cycleEnd_;
        cancel();
      }
      break;
    }
    // Lookahead may reach B early; animate only when the native clock reaches it.
    if(now < cycleEnd_)break;
    const auto boundary=origin_+uint64_t(std::llround((cycleEnd_-cycleStart_)*rate/speed));
    beginReturn(boundary,cycleEnd_,true);
    return;
  }
  now = position(frame);
  if (now < lastScorePosition_) {
    group_ = std::lower_bound(groups_.begin(), groups_.end(),
                              tickSeconds(song_, config_["a"]),
                              [](auto &g, double p) { return g.seconds < p; }) -
             groups_.begin();
    attempt_.clear();
    groupFailed_ = false;
  }
  lastScorePosition_ = now;
  if (config_["mode"] == "wait" || config_["practiceEnabled"] != true || group_ >= groups_.size())
    return;
  while (group_ < groups_.size() &&
         inSelection(groups_[group_]) &&
         now > groups_[group_].seconds +
                   config_["toleranceMs"].get<double>() * .001 * speed) {
    fail(frame);
    if (retry_)
      return;
    ++group_;
    groupFailed_ = false;
    attempt_.clear();
  }
}
void Transport::beginReturn(uint64_t frame,double from,bool play) {
  cancel();
  returnFrom_=from;
  returnTo_=tickSeconds(song_,config_["a"]);
  returnStart_=frame;returnPlay_=play;returning_=true;
  position_=from;retry_=groupFailed_=false;
}
void Transport::fail(uint64_t frame) {
  if (!groupFailed_)
    ++misses_;
  groupFailed_ = true;
  if (config_["mode"] == "pauseOnMiss" && group_ < groups_.size()) {
    position_ = groups_[group_].seconds;
    cancel();
    retry_ = true;
  }
}
void Transport::input(const InputEvent &e) {
  auto key =
      std::tuple<unsigned, unsigned, uint64_t>{e.source, e.channel, e.instance};
  if (e.type == EventType::Panic || e.type == EventType::ResetLive) {
    held_.clear();
    attempt_.clear();
    return;
  }
  if (e.type == EventType::Disconnect) {
    for (auto it = held_.begin(); it != held_.end();)
      if (std::get<0>(it->first) == e.source)
        it = held_.erase(it);
      else
        ++it;
    attempt_.clear();
    return;
  }
  if (e.bus != Bus::Live)
    return;
  if (e.type == EventType::Off) {
    held_.erase(key);
    return;
  }
  if (e.type != EventType::On)
    return;
  held_[key] = e.note;
  if(returning_)return;
  if ((config_["practiceEnabled"] != true && config_["mode"] != "wait") || (!playing_ && !retry_) ||
      group_ >= groups_.size() || !inSelection(groups_[group_]))
    return;
  if (config_["mode"] == "wait" && (!retry_ || e.frame < waitFrom_))
    return;
  auto &g = groups_[group_];
  double time = retry_ ? g.seconds : position(e.frame),
         tolerance = config_["toleranceMs"].get<double>() * .001 *
                     config_["speed"].get<double>();
  if (!g.pitches.contains(e.note) ||
      (!retry_ && std::abs(time - g.seconds) > tolerance)) {
    fail(e.frame);
    attempt_.clear();
    return;
  }
  attempt_[e.note] = e.frame;
  std::set<unsigned> pressed;
  for (auto &[owner, pitch] : held_)
    pressed.insert(pitch);
  if (!std::includes(pressed.begin(), pressed.end(), g.pitches.begin(),
                     g.pitches.end()))
    return;
  uint64_t first = UINT64_MAX, last = 0;
  for (auto pitch : g.pitches) {
    if (!attempt_.contains(pitch))
      return;
    first = std::min(first, attempt_[pitch]);
    last = std::max(last, attempt_[pitch]);
  }
  if (last - first > engine_.sampleRate() * .150) {
    fail(e.frame);
    attempt_.clear();
    return;
  }
  ++hits_;
  ++group_;
  groupFailed_ = false;
  attempt_.clear();
  if (retry_) {
    try {
      resume(e.frame);
    } catch (const std::exception &) {
      return;
    }
    group_ = std::upper_bound(groups_.begin(), groups_.end(), position_,
                              [](double v, auto &g) { return v < g.seconds; }) -
             groups_.begin();
  }
}
void Transport::recover(uint64_t frame) {
  position_ = position(frame);
  cancel();
  held_.clear();
  error_ = "Input capture overflow; stop and restart song practice.";
  retry_ = false;
}
json Transport::snapshot(uint64_t frame) const {
  json cc = json::object();
  double pos = song_.is_null() ? 0 : position(frame),
         tick = song_.is_null() ? 0 : secondsTick(song_, pos);
  for (unsigned channel = 0; channel < 16; ++channel)
    for (unsigned controller = 0; controller < 128; ++controller) {
      auto &values = controls_[channel][controller];
      auto it = std::upper_bound(values.begin(), values.end(), tick,
                                 [](double t, auto &v) { return t < v.tick; });
      if (it != values.begin())
        cc[std::to_string(channel)][std::to_string(controller)] =
            std::prev(it)->value;
    }
  double micro = 500000;
  if (!song_.is_null()) {
    double tick = secondsTick(song_, pos);
    for (auto &entry : song_["tempoMap"])
      if (entry["tick"].get<double>() <= tick)
        micro = entry["microseconds"];
  }
  json hintKeys = json::array();
  const Group *hintGroup = nullptr;
  if (config_["mode"] == "wait") {
    if (group_ < groups_.size()) hintGroup = &groups_[group_];
  } else {
    const auto next = std::lower_bound(hintGroups_.begin(), hintGroups_.end(), pos,
                                      [](const auto &g, double p) { return g.seconds < p; });
    if (next != hintGroups_.end()) hintGroup = &*next;
  }
  if (!returning_ && config_["hints"] == true && hintGroup && inSelection(*hintGroup))
    for (auto &[pitch, hand] : hintGroup->hands)
      hintKeys.push_back({{"midiNote", pitch}, {"hand", hand == 1 ? "left" : hand == 2 ? "right" : "both"}});
  return {{"songId",
           song_.is_null() ? std::string{} : song_.value("id", std::string{})},
          {"title", song_.is_null() ? std::string{}
                                    : song_.value("title", std::string{})},
          {"generation", generation_},
          {"pumpEntries", pumpEntries_},
          {"revision", revision_},
          {"sampleFrame", frame},
          {"sampleRate", engine_.sampleRate()},
          {"songTick", song_.is_null() ? 0 : secondsTick(song_, pos)},
          {"songSeconds", pos},
          {"state", returning_                  ? "returning"
                    : playing_                    ? "playing"
                    : position_ == 0 && !retry_ ? "stopped"
                                                : "paused"},
          {"speed", config_["speed"]},
          {"return", returning_ ? json{{"from",returnFrom_},{"to",returnTo_},
            {"startFrame",returnStart_},{"durationFrames",engine_.sampleRate()*2},
            {"resume",returnPlay_}} : json(nullptr)},
          {"config", config_},
          {"retry", retry_},
          {"waiting", retry_ && config_["mode"] == "wait"},
          {"hintKeys", hintKeys},
          {"hits", hits_},
          {"misses", misses_},
          {"accuracy", hits_ + misses_ ? 100.0 * hits_ / (hits_ + misses_) : 0},
          {"group", group_},
          {"target", !returning_ && group_ < groups_.size() && inSelection(groups_[group_]) ? json(groups_[group_].pitches)
                                             : json::array()},
          {"error", error_},
          {"accompanimentError", accompanimentError_},
          {"followError", followError_},
          {"tempo", 60000000.0 / micro * config_["speed"].get<double>()},
          {"channelControllers", cc}};
}
json songEvents(const json &song, double from, double to, unsigned rate,
                unsigned limit) {
  json result = json::array();
  for (auto &n : song["notes"]) {
    if (n["startTick"].get<double>() > to || n["endTick"].get<double>() < from)
      continue;
    if (result.size() >= limit)
      break;
    auto v = n;
    v["startFrame"] = std::llround(tickSeconds(song, n["startTick"]) * rate);
    v["endFrame"] = std::llround(tickSeconds(song, n["endTick"]) * rate);
    auto pitch = n["midiNote"].get<int>();
    SpelledPitch p{pitch, 'c', 0, pitch / 12 - 1};
    static const char letters[] = {'c', 'c', 'd', 'd', 'e', 'f',
                                   'f', 'g', 'g', 'a', 'a', 'b'};
    static const int accidental[] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
    p.letter = letters[pitch % 12];
    p.accidental = accidental[pitch % 12];
    v["spelling"] = p.notation();
    auto staff = song["tracks"][n["trackId"].get<unsigned>()]["staff"];
    v["staff"] = staff == "bass"     ? "bass"
                 : staff == "treble" ? "treble"
                 : pitch < 60        ? "bass"
                                     : "treble";
    double quarters =
        (n["endTick"].get<double>() - n["startTick"].get<double>()) /
        song["ppq"].get<double>();
    v["duration"] = quarters >= 4    ? "w"
                    : quarters >= 2  ? "h"
                    : quarters >= 1  ? "q"
                    : quarters >= .5 ? "8"
                                     : "16";
    v["highlight"] = "target";
    result.push_back(v);
  }
  return result;
}
json Transport::events(double from, double to, unsigned limit) const {
  return !window_
             ? json::array()
             : window_->query(from, to, engine_.sampleRate(), limit);
}
SongWindow::SongWindow(json song) : metadata_(std::move(song)) {
  prepareSongTiming(metadata_);
  auto meterMap = metadata_.value("meterMap", json::array());
  if (meterMap.empty() || meterMap[0]["tick"].get<double>() > 0)
    meterMap.insert(meterMap.begin(), json{{"tick", 0}, {"numerator", 4}, {"denominator", 4}});
  uint64_t number = 1;
  for (size_t i = 0; i < meterMap.size(); ++i) {
    const double start = meterMap[i]["tick"],
                 end = i + 1 < meterMap.size() ? meterMap[i + 1]["tick"].get<double>() : std::numeric_limits<double>::infinity(),
                 bar = ppq() * 4.0 * meterMap[i]["numerator"].get<double>() / meterMap[i]["denominator"].get<double>();
    if (!std::isfinite(start) || start < 0 || end < start || !std::isfinite(bar) || bar <= 0)
      throw std::invalid_argument("Invalid meter map.");
    if (end == start) continue;
    meters_.push_back({start, end, bar, number});
    if (std::isfinite(end)) {
      const double bars = std::ceil((end - start) / bar);
      if (bars >= double(UINT64_MAX - number)) throw std::invalid_argument("Too many song measures.");
      number += uint64_t(bars);
    }
  }
  notes_ = std::move(metadata_["notes"]); metadata_.erase("notes"); metadata_.erase("rawEvents"); metadata_.erase("controllers");
  order_.resize(notes_.size()); std::iota(order_.begin(),order_.end(),0);
  std::stable_sort(order_.begin(),order_.end(),[&](size_t a,size_t b){return notes_[a]["startTick"].get<double>()<notes_[b]["startTick"].get<double>();});
  base_=1; while(base_<order_.size())base_*=2;
  ends_.assign(base_*2,-1);
  for(size_t i=0;i<order_.size();i++)ends_[base_+i]=notes_[order_[i]]["endTick"].get<double>();
  for(size_t i=base_;--i;)ends_[i]=std::max(ends_[i*2],ends_[i*2+1]);
}
void SongWindow::collect(size_t node,size_t left,size_t right,double from,double to,std::vector<size_t> &found,unsigned limit) const {
  ++inspected_;
  if(found.size()>=limit || left>=order_.size() || ends_[node]<from || notes_[order_[left]]["startTick"].get<double>()>to)return;
  if(right-left==1){found.push_back(order_[left]);return;}
  const auto middle=(left+right)/2;
  collect(node*2,left,middle,from,to,found,limit); collect(node*2+1,middle,right,from,to,found,limit);
}
json SongWindow::query(double from,double to,unsigned rate,unsigned limit) const {
  inspected_=0; std::vector<size_t> found; collect(1,0,base_,from,to,found,limit);
  std::sort(found.begin(),found.end()); auto selected=metadata_; selected["notes"]=json::array();
  for(auto i:found)selected["notes"].push_back(notes_[i]);
  return songEvents(selected,from,to,rate,limit);
}
json SongWindow::measures(double from, double to, unsigned rate) const {
  if (!std::isfinite(from) || !std::isfinite(to) || from < 0 || to < from || !rate)
    throw std::invalid_argument("Invalid measure window.");
  to = std::min(to, metadata_.value("durationTicks", to));
  json result = json::array();
  auto section = std::upper_bound(meters_.begin(), meters_.end(), from,
                                 [](double tick, const MeterSection &m) { return tick < m.start; });
  if (section != meters_.begin()) --section;
  for (; section != meters_.end() && section->start <= to; ++section) {
    const auto &m = *section;
    const double first = std::max(0.0, std::ceil((from - m.start) / m.barTicks)),
                 last = std::min(std::floor((to - m.start) / m.barTicks),
                                 std::isfinite(m.end) ? std::ceil((m.end - m.start) / m.barTicks) - 1 : std::numeric_limits<double>::infinity());
    if (last < first) continue;
    if (last - first + 1 > 4096 - result.size() || last >= double(UINT64_MAX - m.number))
      throw std::invalid_argument("Measure window exceeds 4096 markers.");
    for (uint64_t bar = uint64_t(first); bar <= uint64_t(last); ++bar) {
      const double tick = m.start + double(bar) * m.barTicks;
      const double frame = tickSeconds(metadata_, tick) * rate;
      if (!std::isfinite(frame) || frame >= double(INT64_MAX))
        throw std::invalid_argument("Measure frame exceeds native clock range.");
      result.push_back({{"number", m.number + bar}, {"startFrame", std::llround(frame)}});
    }
  }
  return result;
}
} // namespace sightreading
