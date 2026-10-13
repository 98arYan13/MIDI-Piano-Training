#include <fstream>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <map>
#include <set>
#include <sightreading/FingeringData.hpp>
#include <sightreading/Storage.hpp>
namespace sightreading {
using json = nlohmann::json;
namespace {
bool eligibleFingeringNote(const json &song, const json &note) {
  const auto trackId = note.at("trackId").get<size_t>();
  const auto pitch = note.at("midiNote").get<int>();
  return trackId < song.at("tracks").size() && pitch >= 21 && pitch <= 108 &&
         !(note.contains("channel") && note["channel"] == 9) &&
         !song["tracks"][trackId].value("percussion", false);
}
bool validNoteId(const json &value) {
  return value.is_number_integer() && value.get<int64_t>() >= 0;
}
}
json Storage::song(const std::string &id) {
  std::lock_guard lock(mutex);
  auto q = prepare("SELECT model,trashed FROM songs WHERE id=?");
  bind(q.get(), 1, id);
  if (sqlite3_step(q.get()) != SQLITE_ROW)
    throw std::invalid_argument("Song not found.");
  auto s = json::parse(
      reinterpret_cast<const char *>(sqlite3_column_text(q.get(), 0)));
  s["trashed"] = sqlite3_column_int(q.get(), 1) != 0;
  if (!s.contains("fingeringRevision")) s["fingeringRevision"] = 0;
  return s;
}
void Storage::putSong(const json &model, const std::string &original) {
  const auto id = model.at("id").get<std::string>();
  if (id.empty() ||
      id.find_first_not_of(
          "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-") !=
          std::string::npos)
    throw std::invalid_argument("Invalid song identity.");
  std::lock_guard lock(mutex);
  auto folder = directory / "originals";
  std::filesystem::create_directories(folder);
  auto path = folder / (id + ".mid");
  auto staged = folder / (id + ".tmp");
  auto stored = model;
  if (!stored.contains("fingeringRevision")) stored["fingeringRevision"] = 0;
  bool moved = false;
  sql("BEGIN IMMEDIATE");
  try {
    auto q = prepare("INSERT INTO songs(id,title,model) VALUES(?,?,?)");
    bind(q.get(), 1, id);
    bind(q.get(), 2, stored.at("title"));
    bind(q.get(), 3, stored.dump());
    if (sqlite3_step(q.get()) != SQLITE_DONE)
      throw std::runtime_error(sqlite3_errmsg(db));
    auto o = prepare("INSERT INTO song_originals(id,bytes) VALUES(?,?)");
    bind(o.get(), 1, id);
    sqlite3_bind_blob(o.get(), 2, original.data(), int(original.size()),
                      SQLITE_TRANSIENT);
    if (sqlite3_step(o.get()) != SQLITE_DONE)
      throw std::runtime_error(sqlite3_errmsg(db));
    {
      std::ofstream f(staged, std::ios::binary | std::ios::trunc);
      f.write(original.data(), original.size());
      f.flush();
      if (!f)
        throw std::runtime_error("Cannot preserve original MIDI file.");
    }
    std::filesystem::rename(staged, path);
    moved = true;
    sql("COMMIT");
    ++libraryRevision_;
  } catch (...) {
    sql("ROLLBACK");
    std::error_code ec;
    std::filesystem::remove(staged, ec);
    if (moved)
      std::filesystem::remove(path, ec);
    throw;
  }
}
void Storage::patchSong(const std::string &id, const json &patch) {
  // Serialize read-modify-write under the same mutex: no stale track toggles.
  std::lock_guard lock(mutex);
  auto q = prepare("SELECT model FROM songs WHERE id=?");
  bind(q.get(), 1, id);
  if (sqlite3_step(q.get()) != SQLITE_ROW)
    throw std::invalid_argument("Song not found.");
  auto model = json::parse(
      reinterpret_cast<const char *>(sqlite3_column_text(q.get(), 0)));
  // End the read snapshot before writing: a concurrent settings checkpoint
  // otherwise makes SQLite reject the read-to-write upgrade (BUSY_SNAPSHOT).
  q.reset();
  if (!patch.is_object())
    throw std::invalid_argument("Expected song edit object.");
  const auto oldFingerprint = patch.contains("tracks") ? fingeringFingerprint(model) : std::string{};
  for (auto it = patch.begin(); it != patch.end(); ++it) {
    if (it.key() == "title") {
      if (!it.value().is_string() || it.value().get<std::string>().empty() ||
          it.value().get<std::string>().size() > 200)
        throw std::invalid_argument("Title needs 1–200 characters.");
      model["title"] = it.value();
    } else if (it.key() == "recent") {
      if (!it.value().is_number_unsigned())
        throw std::invalid_argument("Invalid recent time.");
      model["recent"] = it.value();
    } else if (it.key() == "tracks") {
      if (!it.value().is_array())
        throw std::invalid_argument("Tracks must be assignments array.");
      for (auto &edit : it.value()) {
        auto tid = edit.at("id").get<unsigned>();
        if (tid >= model["tracks"].size())
          throw std::invalid_argument("Track not found.");
        auto &track = model["tracks"][tid];
        for (auto x = edit.begin(); x != edit.end(); ++x) {
          if (x.key() == "id")
            continue;
          if (x.key() == "practice" || x.key() == "backing") {
            if (!x.value().is_boolean())
              throw std::invalid_argument("Track switches must be boolean.");
            track[x.key()] = x.value();
          } else if (x.key() == "staff") {
            if (x.value() != "split" && x.value() != "treble" &&
                x.value() != "bass" && x.value() != "grand")
              throw std::invalid_argument("Unknown staff assignment.");
            track["staff"] = x.value();
          } else
            throw std::invalid_argument("Unknown track assignment.");
        }
      }
    } else
      throw std::invalid_argument("Unknown song field.");
  }
  if (patch.contains("tracks") && fingeringFingerprint(model) != oldFingerprint) {
    for (auto &note : model["notes"]) if (eligibleFingeringNote(model, note) && note.contains("fingering"))
      note["fingering"]["hand"] = resolvedFingeringHand(model, note);
    assignFingeringTips(model);
    model["fingeringRevision"] = model.value("fingeringRevision", uint64_t(0)) + 1;
  }
  auto update = prepare("UPDATE songs SET title=?,model=? WHERE id=?");
  bind(update.get(), 1, model["title"]);
  bind(update.get(), 2, model.dump());
  bind(update.get(), 3, id);
  if (sqlite3_step(update.get()) != SQLITE_DONE)
    throw std::runtime_error(sqlite3_errmsg(db));
  ++libraryRevision_;
}

json Storage::commitFingering(const std::string &id,
                             const std::string &expectedFingerprint,
                             const json &result) {
  std::lock_guard lock(mutex);
  sql("BEGIN IMMEDIATE");
  try {
    auto q = prepare("SELECT model,trashed FROM songs WHERE id=?");
    bind(q.get(), 1, id);
    if (sqlite3_step(q.get()) != SQLITE_ROW) throw std::invalid_argument("Song not found.");
    auto model = json::parse(reinterpret_cast<const char *>(sqlite3_column_text(q.get(), 0)));
    const bool trashed = sqlite3_column_int(q.get(), 1) != 0;
    q.reset();
    if (trashed) throw std::invalid_argument("Restore this song from trash first.");
    if (fingeringFingerprint(model) != expectedFingerprint)
      throw std::invalid_argument("Song changed during fingering generation; discard stale results.");
    if (!result.is_object() || !result.contains("model") || !result.contains("assignments") || !result["assignments"].is_array())
      throw std::invalid_argument("Invalid fingering result.");
    const auto &metadata = result["model"];
    if (!metadata.is_object() || metadata.value("name", std::string{}) != "ArGNN" ||
        !metadata.contains("revision") || !metadata["revision"].is_string() || metadata["revision"].get<std::string>().empty() ||
        !metadata.contains("checkpointHashes") || !metadata["checkpointHashes"].is_object())
      throw std::invalid_argument("Invalid fingering model metadata.");
    const auto &hashes = metadata["checkpointHashes"];
    auto validHash = [](const json &v) {
      if (!v.is_string() || v.get<std::string>().size() != 64) return false;
      const auto text = v.get<std::string>();
      return std::all_of(text.begin(), text.end(), [](unsigned char c) { return std::isxdigit(c); });
    };
    if (!hashes.contains("left") || !hashes.contains("right") || !validHash(hashes["left"]) || !validHash(hashes["right"]))
      throw std::invalid_argument("Invalid fingering checkpoint hashes.");
    std::map<uint64_t, size_t> eligibleNotes;
    for (size_t i = 0; i < model["notes"].size(); ++i)
      if (eligibleFingeringNote(model, model["notes"][i])) eligibleNotes.emplace(model["notes"][i].at("id").get<uint64_t>(), i);
    std::map<uint64_t, json> assignments;
    for (const auto &entry : result["assignments"]) {
      if (!entry.is_object() || !entry.contains("id") || !validNoteId(entry["id"]) ||
          !entry.contains("finger") || !(entry["finger"].is_null() || (entry["finger"].is_number_integer() && entry["finger"].get<int>() >= 1 && entry["finger"].get<int>() <= 5)) ||
          !entry.contains("hand") || !entry["hand"].is_string() ||
          (entry["hand"] != "left" && entry["hand"] != "right") ||
          (entry.contains("reason") && !entry["reason"].is_string()))
        throw std::invalid_argument("Invalid fingering assignment.");
      if (entry.contains("source") && entry["source"] != "generated")
        throw std::invalid_argument("Invalid fingering assignment source.");
      const auto noteId = entry["id"].get<uint64_t>();
      if (!eligibleNotes.contains(noteId)) throw std::invalid_argument("Fingering result contains an unknown or unsupported note.");
      if (!assignments.emplace(noteId, entry).second) throw std::invalid_argument("Fingering result contains duplicate note IDs.");
      if (entry["hand"] != resolvedFingeringHand(model, model["notes"][eligibleNotes.at(noteId)]))
        throw std::invalid_argument("Fingering hand does not match score grouping.");
    }
    if (assignments.size() != eligibleNotes.size()) throw std::invalid_argument("Fingering result is missing eligible notes.");
    size_t assigned = 0, unresolved = 0;
    for (auto &[noteId, index] : eligibleNotes) {
      auto &note = model["notes"][index];
      const auto &entry = assignments.at(noteId);
      if (note.contains("fingering") && note["fingering"].value("source", std::string{}) == "manual") {
        auto manual = note["fingering"];
        if (manual.value("finger", json(nullptr)) != entry["finger"])
          throw std::invalid_argument("Fingering result changed a manual anchor.");
        manual["hand"] = entry["hand"];
        if (entry.contains("reason")) manual["reason"] = entry["reason"];
        note["fingering"] = std::move(manual);
      } else {
        json fingering = {{"finger", entry["finger"]}, {"hand", entry["hand"]}, {"source", "generated"}};
        if (entry.contains("reason")) fingering["reason"] = entry["reason"];
        note["fingering"] = std::move(fingering);
      }
      if (entry["finger"].is_null()) ++unresolved; else ++assigned;
    }
    assignFingeringTips(model);
    const auto revision = model.value("fingeringRevision", uint64_t(0)) + 1;
    model["fingeringRevision"] = revision;
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm utc{};
    gmtime_s(&utc, &now);
    std::ostringstream generatedAt;
    generatedAt << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    model["fingeringRun"] = {{"model", metadata}, {"generatedAt", generatedAt.str()},
      {"fingerprint", expectedFingerprint}, {"assigned", assigned},
      {"unresolved", unresolved}, {"unsupported", model["notes"].size() - eligibleNotes.size()}};
    auto update = prepare("UPDATE songs SET title=?,model=? WHERE id=?");
    bind(update.get(), 1, model["title"]);
    bind(update.get(), 2, model.dump());
    bind(update.get(), 3, id);
    if (sqlite3_step(update.get()) != SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(db));
    sql("COMMIT");
    ++libraryRevision_;
    return model;
  } catch (...) {
    sql("ROLLBACK");
    throw;
  }
}

json Storage::editFingering(const std::string &id,
                            const std::vector<uint64_t> &noteIds,
                            std::optional<int> finger) {
  std::lock_guard lock(mutex);
  if (finger && (*finger < 1 || *finger > 5)) throw std::invalid_argument("Finger must be between 1 and 5.");
  if (noteIds.empty()) throw std::invalid_argument("Choose at least one note.");
  std::set<uint64_t> unique(noteIds.begin(), noteIds.end());
  if (unique.size() != noteIds.size()) throw std::invalid_argument("Duplicate fingering note ID.");
  auto q = prepare("SELECT model,trashed FROM songs WHERE id=?");
  bind(q.get(), 1, id);
  if (sqlite3_step(q.get()) != SQLITE_ROW) throw std::invalid_argument("Song not found.");
  auto model = json::parse(reinterpret_cast<const char *>(sqlite3_column_text(q.get(), 0)));
  if (sqlite3_column_int(q.get(), 1)) throw std::invalid_argument("Restore this song from trash first.");
  q.reset();
  std::map<uint64_t, size_t> indices;
  for (size_t i = 0; i < model["notes"].size(); ++i)
    if (eligibleFingeringNote(model, model["notes"][i])) indices.emplace(model["notes"][i].at("id").get<uint64_t>(), i);
  for (auto noteId : unique) if (!indices.contains(noteId)) throw std::invalid_argument("Unknown or unsupported fingering note ID.");
  for (auto noteId : unique) {
    auto &note = model["notes"][indices.at(noteId)];
    const auto hand = resolvedFingeringHand(model, note);
    json current = note.value("fingering", json::object());
    if (!current.is_object()) current = json::object();
    if (finger) {
      const json automatic = current.contains("automaticFinger") ? current["automaticFinger"] :
          (current.contains("finger") && current.value("source", std::string{}) == "generated" ? current["finger"] : json(nullptr));
      current = {{"finger", *finger}, {"hand", hand}, {"source", "manual"}};
      if (automatic.is_number_integer() && automatic.get<int>() >= 1 && automatic.get<int>() <= 5)
        current["automaticFinger"] = automatic;
    } else if (current.value("source", std::string{}) == "manual") {
      if (current.contains("automaticFinger") && current["automaticFinger"].is_number_integer() &&
          current["automaticFinger"].get<int>() >= 1 && current["automaticFinger"].get<int>() <= 5)
        current = {{"finger", current["automaticFinger"]}, {"hand", hand}, {"source", "generated"}};
      else current = nullptr;
    }
    if (current.is_null() || current.empty()) note.erase("fingering");
    else note["fingering"] = std::move(current);
  }
  assignFingeringTips(model);
  model["fingeringRevision"] = model.value("fingeringRevision", uint64_t(0)) + 1;
  auto update = prepare("UPDATE songs SET title=?,model=? WHERE id=?");
  bind(update.get(), 1, model["title"]);
  bind(update.get(), 2, model.dump());
  bind(update.get(), 3, id);
  if (sqlite3_step(update.get()) != SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(db));
  ++libraryRevision_;
  return model;
}
void Storage::trashSong(const std::string &id, bool trashed) {
  std::lock_guard lock(mutex);
  auto q = prepare("UPDATE songs SET trashed=? WHERE id=?");
  sqlite3_bind_int(q.get(), 1, trashed);
  bind(q.get(), 2, id);
  if (sqlite3_step(q.get()) != SQLITE_DONE)
    throw std::runtime_error(sqlite3_errmsg(db));
  if (!sqlite3_changes(db))
    throw std::invalid_argument("Song not found.");
  ++libraryRevision_;
}
std::string Storage::originalSong(const std::string &id) {
  std::lock_guard lock(mutex);
  auto q = prepare("SELECT bytes FROM song_originals WHERE id=?");
  bind(q.get(), 1, id);
  if (sqlite3_step(q.get()) != SQLITE_ROW)
    throw std::invalid_argument("Original MIDI not found.");
  return std::string(static_cast<const char *>(sqlite3_column_blob(q.get(), 0)),
                     sqlite3_column_bytes(q.get(), 0));
}
json Storage::trashSongs() {
  std::lock_guard lock(mutex);
  auto q = prepare("SELECT model FROM songs WHERE trashed=1 ORDER BY title");
  json result = json::array();
  while (sqlite3_step(q.get()) == SQLITE_ROW) {
    auto s = json::parse(
        reinterpret_cast<const char *>(sqlite3_column_text(q.get(), 0)));
    if (!s.contains("fingeringRevision")) s["fingeringRevision"] = 0;
    s.erase("notes");
    s.erase("controllers");
    s.erase("rawEvents");
    s["trashed"] = true;
    result.push_back(s);
  }
  return result;
}
} // namespace sightreading
