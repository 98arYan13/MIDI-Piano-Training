#include <sightreading/FingeringData.hpp>
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace sightreading {
using json = nlohmann::json;
namespace {
bool isStarterTrackName(std::string title, const std::string &expected) {
  std::transform(title.begin(), title.end(), title.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  std::string lowered = expected;
  std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return title == lowered;
}
bool hasWord(const std::string &text, const std::string &word) {
  std::string lower = text;
  std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  size_t at = 0;
  while ((at = lower.find(word, at)) != std::string::npos) {
    const auto wordChar = [](unsigned char c) { return std::isalnum(c) || c == '_'; };
    const bool left = at == 0 || !wordChar(static_cast<unsigned char>(lower[at - 1]));
    const auto end = at + word.size();
    const bool right = end == lower.size() || !wordChar(static_cast<unsigned char>(lower[end]));
    if (left && right) return true;
    at = end;
  }
  return false;
}
bool eligible(const json &song, const json &note) {
  const auto pitch = note.at("midiNote").get<int>();
  const auto trackId = note.at("trackId").get<size_t>();
  if (trackId >= song.at("tracks").size() || pitch < 21 || pitch > 108 ||
      (note.contains("channel") && note["channel"] == 9)) return false;
  return !song["tracks"][trackId].value("percussion", false);
}
int whiteKey(int midi) {
  static constexpr std::array<int, 12> before = {0, 1, 1, 2, 2, 3, 4, 4, 5, 5, 6, 6};
  const int octave = midi / 12;
  const int pc = midi % 12;
  return octave * 7 + before[pc] + (pc == 0 || pc == 2 || pc == 4 || pc == 5 || pc == 7 || pc == 9 || pc == 11 ? 1 : 0);
}
std::string sha256(const std::string &data) {
  BCRYPT_ALG_HANDLE algorithm{};
  BCRYPT_HASH_HANDLE hash{};
  DWORD objectBytes{}, resultBytes{};
  std::vector<UCHAR> object;
  std::array<UCHAR, 32> digest{};
  auto check = [](NTSTATUS status, const char *operation) {
    if (status < 0) throw std::runtime_error(std::string("BCrypt ") + operation + " failed.");
  };
  check(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0), "algorithm initialization");
  try {
    check(BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectBytes), sizeof(objectBytes), &resultBytes, 0), "object sizing");
    object.resize(objectBytes);
    check(BCryptCreateHash(algorithm, &hash, object.data(), objectBytes, nullptr, 0, 0), "hash initialization");
    if (data.size() > std::numeric_limits<ULONG>::max()) throw std::runtime_error("Fingering fingerprint input is too large.");
    check(BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char *>(data.data())), static_cast<ULONG>(data.size()), 0), "hash input");
    check(BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0), "hash finalization");
  } catch (...) {
    if (hash) BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    throw;
  }
  BCryptDestroyHash(hash);
  BCryptCloseAlgorithmProvider(algorithm, 0);
  std::ostringstream out;
  out << std::hex << std::setfill('0');
  for (auto byte : digest) out << std::setw(2) << unsigned(byte);
  return out.str();
}
struct Attack {
  int64_t tick{};
  int64_t end{};
  std::vector<json *> notes;
};
void addTip(json &note, const char *tip) {
  auto &tips = note["fingering"]["tips"];
  if (std::find(tips.begin(), tips.end(), tip) == tips.end()) tips.push_back(tip);
}
}

std::string resolvedFingeringHand(const json &song, const json &note) {
  const auto &tracks = song.at("tracks");
  const auto id = note.at("trackId").get<size_t>();
  if (id >= tracks.size()) throw std::invalid_argument("Fingering note has an unknown track.");
  const auto &track = tracks[id];
  const auto staff = track.value("staff", std::string{});
  if (staff == "treble") return "right";
  if (staff == "bass") return "left";
  const auto title = track.value("title", std::string{});
  if (hasWord(title, "lh") || hasWord(title, "left") || isStarterTrackName(title, "Piano Backing")) return "left";
  if (hasWord(title, "rh") || hasWord(title, "right") || isStarterTrackName(title, "Practice Melody")) return "right";
  const auto noteStaff = note.value("staff", std::string{});
  if (noteStaff == "bass") return "left";
  if (noteStaff == "treble") return "right";
  return note.at("midiNote").get<int>() < 60 ? "left" : "right";
}

std::string fingeringFingerprint(const json &song) {
  json records = json::array();
  std::vector<const json *> notes;
  for (const auto &note : song.at("notes")) if (eligible(song, note)) notes.push_back(&note);
  std::sort(notes.begin(), notes.end(), [](const json *a, const json *b) { return a->at("id") < b->at("id"); });
  for (const auto *note : notes) {
    json anchor = nullptr;
    if (note->contains("fingering") && note->at("fingering").value("source", std::string{}) == "manual")
      anchor = note->at("fingering").value("finger", json(nullptr));
    records.push_back({{"id", note->at("id")}, {"midiNote", note->at("midiNote")},
      {"startTick", note->at("startTick")}, {"endTick", note->at("endTick")},
      {"hand", resolvedFingeringHand(song, *note)}, {"manualFinger", anchor}});
  }
  return sha256(records.dump());
}

json fingeringRequest(const json &song, const std::string &jobId) {
  json notes = json::array();
  for (const auto &note : song.at("notes")) {
    if (!eligible(song, note)) continue;
    json manual = nullptr;
    if (note.contains("fingering") && note["fingering"].value("source", std::string{}) == "manual")
      manual = note["fingering"].value("finger", json(nullptr));
    notes.push_back({{"id", note.at("id")}, {"midiNote", note.at("midiNote")},
      {"startTick", note.at("startTick")}, {"endTick", note.at("endTick")},
      {"hand", resolvedFingeringHand(song, note)}, {"manualFinger", manual}});
  }
  return {{"version", 1}, {"jobId", jobId}, {"songId", song.at("id")},
    {"fingerprint", fingeringFingerprint(song)}, {"ppq", song.at("ppq")}, {"notes", notes}};
}

void assignFingeringTips(json &song) {
  std::map<std::string, std::vector<Attack>> byHand;
  for (auto &note : song["notes"]) {
    if (!eligible(song, note)) continue;
    if (note.contains("fingering")) note["fingering"]["tips"] = json::array();
    const auto hand = resolvedFingeringHand(song, note);
    auto &events = byHand[hand];
    const auto tick = note.at("startTick").get<int64_t>();
    auto it = std::find_if(events.begin(), events.end(), [tick](const Attack &a) { return a.tick == tick; });
    if (it == events.end()) { events.push_back({tick, note.at("endTick").get<int64_t>(), {&note}}); }
    else { it->end = std::max(it->end, note.at("endTick").get<int64_t>()); it->notes.push_back(&note); }
  }
  const auto ppq = song.value("ppq", uint64_t(480));
  for (auto &[hand, events] : byHand) {
    std::sort(events.begin(), events.end(), [](const Attack &a, const Attack &b) { return a.tick < b.tick; });
    int64_t depressedUntil = 0;
    bool hasPreviousEvent = false;
    std::optional<double> previousOrigin;
    json *previousNote = nullptr;
    int previousPitch = 0, previousFinger = 0;
    const int direction = hand == "right" ? 1 : -1;
    for (auto &event : events) {
      if (!hasPreviousEvent || event.tick - depressedUntil >= static_cast<int64_t>(ppq))
        for (auto *note : event.notes)
          if (note->contains("fingering") && !note->at("fingering").value("finger", json(nullptr)).is_null()) addTip(*note, "phraseStart");
      depressedUntil = std::max(depressedUntil, event.end);
      hasPreviousEvent = true;
      std::set<int> pitches;
      std::vector<double> origins;
      int low = 127, high = 0;
      for (auto *note : event.notes) {
        const int pitch = note->at("midiNote").get<int>();
        pitches.insert(pitch); low = std::min(low, pitch); high = std::max(high, pitch);
        if (note->contains("fingering") && !note->at("fingering").value("finger", json(nullptr)).is_null()) {
          const int finger = note->at("fingering").at("finger").get<int>();
          origins.push_back(whiteKey(pitch) - direction * (finger - 1));
        }
      }
      if (pitches.size() == 1 && event.notes.size() == 1 &&
          event.notes.front()->contains("fingering") &&
          !event.notes.front()->at("fingering").value("finger", json(nullptr)).is_null()) {
        auto *note = event.notes.front();
        const int pitch = note->at("midiNote").get<int>();
        const int finger = note->at("fingering").at("finger").get<int>();
        if (previousNote && pitch != previousPitch && finger != previousFinger &&
            (pitch - previousPitch) * (finger - previousFinger) * direction < 0 &&
            (finger == 1 || previousFinger == 1)) addTip(*note, "thumbCrossing");
        previousNote = note; previousPitch = pitch; previousFinger = finger;
      } else previousNote = nullptr;
      if (pitches.size() >= 4 || high - low >= 12)
        for (auto *note : event.notes)
          if (note->contains("fingering") && !note->at("fingering").value("finger", json(nullptr)).is_null()) addTip(*note, "difficultChord");
      std::sort(origins.begin(), origins.end());
      if (!origins.empty()) {
        const auto middle = origins.size() / 2;
        const double origin = origins.size() % 2 ? origins[middle] : (origins[middle - 1] + origins[middle]) / 2.0;
        if (previousOrigin) {
          const double change = std::abs(origin - *previousOrigin);
          if (change >= 2) for (auto *note : event.notes) addTip(*note, "positionChange");
          if (change >= 4) for (auto *note : event.notes) addTip(*note, "majorPositionChange");
        }
        previousOrigin = origin;
      }
    }
  }
}
} // namespace sightreading
