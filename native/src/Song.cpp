#include <MidiFile.h>
#include <algorithm>
#include <deque>
#include <map>
#include <sightreading/Song.hpp>
#include <sstream>
namespace sightreading {
using json = nlohmann::json;
namespace {
struct Reader {
  const std::string &b;
  size_t p{}, end;
  unsigned byte() {
    if (p >= end)
      throw std::invalid_argument("Truncated MIDI event/chunk.");
    return uint8_t(b[p++]);
  }
  unsigned number(unsigned n) {
    unsigned v = 0;
    while (n--)
      v = (v << 8) | byte();
    return v;
  }
  unsigned vlq() {
    unsigned v = 0;
    for (unsigned i = 0; i < 4; i++) {
      auto c = byte();
      v = (v << 7) | (c & 127);
      if (!(c & 128))
        return v;
    }
    throw std::invalid_argument("MIDI VLQ exceeds four bytes.");
  }
  void tag(const char *t) {
    for (unsigned i = 0; i < 4; i++)
      if (byte() != unsigned(t[i]))
        throw std::invalid_argument("Expected binary MIDI header/track.");
  }
  void skip(unsigned n) {
    if (n > end - p)
      throw std::invalid_argument("Truncated MIDI payload.");
    p += n;
  }
};
void preflight(const std::string &b) {
  if (b.size() > 20 * 1024 * 1024)
    throw std::invalid_argument("MIDI exceeds 20 MB.");
  Reader r{b, 0, b.size()};
  r.tag("MThd");
  if (r.number(4) != 6)
    throw std::invalid_argument("MIDI header must have six bytes.");
  auto format = r.number(2), tracks = r.number(2), ppq = r.number(2);
  if (format > 1)
    throw std::invalid_argument(
        "SMF type 2 is unsupported; export type 0 or 1.");
  if (!tracks || tracks > 256 || (format == 0 && tracks != 1))
    throw std::invalid_argument("MIDI needs 1–256 tracks; type 0 needs one.");
  if (!ppq || (ppq & 0x8000))
    throw std::invalid_argument(
        "SMPTE timing is unsupported; export PPQ MIDI.");
  unsigned events = 0;
  for (unsigned t = 0; t < tracks; t++) {
    r.tag("MTrk");
    auto length = r.number(4);
    if (length > r.end - r.p)
      throw std::invalid_argument("Truncated MIDI track.");
    auto finish = r.p + length;
    Reader e{b, r.p, finish};
    unsigned running = 0;
    uint64_t tick = 0;
    bool ended = false;
    while (e.p < finish) {
      tick += e.vlq();
      if (tick > INT32_MAX)
        throw std::invalid_argument("MIDI tick range exceeded.");
      if (++events > 1000000)
        throw std::invalid_argument("MIDI exceeds one million events.");
      auto status = e.byte();
      if (status < 128) {
        if (!running)
          throw std::invalid_argument("Invalid running status.");
        --e.p;
        status = running;
      }
      if (status == 0xff) {
        running = 0;
        auto type = e.byte(), n = e.vlq();
        auto pos = e.p;
        if (type == 0x2f) {
          if (n || e.p != finish)
            throw std::invalid_argument("Invalid end-of-track.");
          ended = true;
        }
        if (type == 0x51 &&
            (n != 3 || n > finish - pos ||
             !(uint8_t(b[pos]) | uint8_t(b[pos + 1]) | uint8_t(b[pos + 2]))))
          throw std::invalid_argument("Invalid tempo.");
        if (type == 0x58 && (n != 4 || n > finish - pos || !uint8_t(b[pos]) ||
                             uint8_t(b[pos + 1]) > 4))
          throw std::invalid_argument("Invalid meter.");
        if (type == 0x59 &&
            (n != 2 || n > finish - pos || int8_t(b[pos]) < -7 ||
             int8_t(b[pos]) > 7 || uint8_t(b[pos + 1]) > 1))
          throw std::invalid_argument("Invalid key signature.");
        e.skip(n);
      } else if (status == 0xf0 || status == 0xf7) {
        running = 0;
        e.skip(e.vlq());
      } else if (status < 0xf0) {
        running = status;
        unsigned n =
            ((status & 0xf0) == 0xc0 || (status & 0xf0) == 0xd0) ? 1 : 2;
        while (n--)
          if (e.byte() > 127)
            throw std::invalid_argument("Invalid MIDI channel data.");
      } else
        throw std::invalid_argument("Invalid SMF status.");
    }
    if (!ended)
      throw std::invalid_argument("MIDI track lacks end-of-track.");
    r.p = finish;
  }
  if (r.p != r.end)
    throw std::invalid_argument("Unexpected bytes after MIDI tracks.");
}
} // namespace
void prepareSongTiming(json &s) {
  double seconds = 0, previous = 0, micro = 500000, ppq = s["ppq"];
  for (auto &v : s["tempoMap"]) {
    double tick = v["tick"];
    seconds += (tick - previous) * micro / 1000000.0 / ppq;
    v["seconds"] = seconds;
    previous = tick;
    micro = v["microseconds"];
  }
}
double tickSeconds(const json &s, double tick) {
  auto &map = s["tempoMap"];
  if (map[0].contains("seconds")) {
    auto it =
        std::upper_bound(map.begin(), map.end(), tick, [](double t, auto &v) {
          return t < v["tick"].template get<double>();
        });
    if (it != map.begin())
      --it;
    return (*it)["seconds"].get<double>() +
           (tick - (*it)["tick"].get<double>()) *
               (*it)["microseconds"].get<double>() / 1000000.0 /
               s["ppq"].get<double>();
  }
  double seconds = 0, previous = 0, micro = 500000;
  for (auto &v : map) {
    double next = v["tick"];
    if (next > tick)
      break;
    seconds += (next - previous) * micro / 1000000.0 / s["ppq"].get<double>();
    previous = next;
    micro = v["microseconds"];
  }
  return seconds +
         (tick - previous) * micro / 1000000.0 / s["ppq"].get<double>();
}
double secondsTick(const json &s, double seconds) {
  auto &map = s["tempoMap"];
  if (map[0].contains("seconds")) {
    auto it = std::upper_bound(map.begin(), map.end(), seconds,
                               [](double t, auto &v) {
                                 return t < v["seconds"].template get<double>();
                               });
    if (it != map.begin())
      --it;
    return (*it)["tick"].get<double>() +
           (seconds - (*it)["seconds"].get<double>()) * 1000000 *
               s["ppq"].get<double>() / (*it)["microseconds"].get<double>();
  }
  double previous = 0, micro = 500000;
  auto ppq = s["ppq"].get<double>();
  for (auto &v : map) {
    double next = v["tick"], span = (next - previous) * micro / 1000000.0 / ppq;
    if (seconds < span)
      return previous + seconds * 1000000 * ppq / micro;
    seconds -= span;
    previous = next;
    micro = v["microseconds"];
  }
  return previous + seconds * 1000000 * ppq / micro;
}
json importMidi(const std::string &b, const std::string &title) {
  if (title.size() > 200)
    throw std::invalid_argument("MIDI title exceeds 200 characters.");
  preflight(b);
  std::istringstream in(b);
  smf::MidiFile file;
  if (!file.read(in))
    throw std::invalid_argument("Cannot parse MIDI.");
  json s = {
      {"title", title.empty() ? "Untitled MIDI" : title},
      {"ppq", file.getTicksPerQuarterNote()},
      {"tracks", json::array()},
      {"notes", json::array()},
      {"controllers", json::array()},
      {"tempoMap", json::array({{{"tick", 0}, {"microseconds", 500000}}})},
      {"meterMap",
       json::array({{{"tick", 0}, {"numerator", 4}, {"denominator", 4}}})},
      {"keyMap", json::array()},
      {"rawEvents", json::array()}};
  unsigned id = 0, order = 0;
  int lastTick = 0;
  for (int t = 0; t < file.getTrackCount(); t++) {
    std::map<std::pair<unsigned, unsigned>, std::deque<size_t>> held;
    bool drums = false, pitched = false;
    unsigned count = 0;
    std::string name = "Track " + std::to_string(t + 1);
    for (int ei = 0; ei < file[t].size(); ++ei) {
      auto &e = file[t][ei];
      ++order;
      lastTick = std::max(lastTick, e.tick);
      unsigned channel = e[0] & 15;
      json raw = json::array();
      for (auto v : e)
        raw.push_back(v);
      s["rawEvents"].push_back(
          {{"trackId", t}, {"tick", e.tick}, {"order", order}, {"bytes", raw}});
      if (e.isNoteOn()) {
        drums |= channel == 9;
        pitched |= channel != 9;
        ++count;
        held[{channel, e[1]}].push_back(s["notes"].size());
        s["notes"].push_back({{"id", ++id},
                              {"trackId", t},
                              {"channel", channel},
                              {"midiNote", e[1]},
                              {"velocity", e[2]},
                              {"startTick", e.tick},
                              {"endTick", e.tick},
                              {"startOrder", order},
                              {"endOrder", order}});
      } else if (e.isNoteOff()) {
        auto &q = held[{channel, e[1]}];
        if (!q.empty()) {
          s["notes"][q.front()]["endTick"] = e.tick;
          s["notes"][q.front()]["endOrder"] = order;
          q.pop_front();
        }
      } else if (e.isController())
        s["controllers"].push_back({{"trackId", t},
                                    {"channel", channel},
                                    {"tick", e.tick},
                                    {"order", order},
                                    {"controller", e[1]},
                                    {"value", e[2]}});
      else if (e.isTempo())
        s["tempoMap"].push_back(
            {{"tick", e.tick}, {"microseconds", e.getTempoMicroseconds()}});
      else if (e.isTimeSignature())
        s["meterMap"].push_back({{"tick", e.tick},
                                 {"numerator", e[3]},
                                 {"denominator", 1 << e[4]}});
      else if (e.isKeySignature())
        s["keyMap"].push_back({{"tick", e.tick},
                               {"sharps", int(int8_t(e[3]))},
                               {"minor", e[4] != 0}});
      else if (e.isMeta() && e.size() > 3 && e[1] == 3)
        name = e.getMetaContent();
    }
    for (auto &[key, q] : held)
      for (auto n : q)
        s["notes"][n]["endTick"] = file[t][file[t].size() - 1].tick;
    s["tracks"].push_back({{"id", t},
                           {"title", name},
                           {"noteCount", count},
                           {"percussion", drums && !pitched},
                           {"practice", false},
                           {"backing", pitched},
                           {"staff", "split"}});
  }
  for (auto field : {"tempoMap", "meterMap", "keyMap"}) {
    auto &a = s[field];
    std::stable_sort(a.begin(), a.end(),
                     [](auto &a, auto &b) { return a["tick"] < b["tick"]; });
    json unique = json::array();
    for (auto &v : a) {
      if (!unique.empty() && unique.back()["tick"] == v["tick"])
        unique.back() = v;
      else
        unique.push_back(v);
    }
    a = unique;
  }
  if (s["notes"].empty())
    throw std::invalid_argument("MIDI contains no notes.");
  prepareSongTiming(s);
  std::stable_sort(s["controllers"].begin(), s["controllers"].end(),
                   [](auto &a, auto &b) { return a["tick"] < b["tick"]; });
  s["durationTicks"] = lastTick;
  s["durationSeconds"] = tickSeconds(s, lastTick);
  if (s["durationSeconds"].get<double>() > 7200)
    throw std::invalid_argument("MIDI exceeds two hours.");
  for (auto &track : s["tracks"])
    if (track["noteCount"].get<unsigned>() &&
        !track["percussion"].get<bool>()) {
      track["practice"] = true;
      track["backing"] = false;
      break;
    }
  return s;
}
} // namespace sightreading
