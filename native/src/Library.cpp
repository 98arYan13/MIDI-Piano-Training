#include <fstream>
#include <sightreading/Storage.hpp>
namespace sightreading {
using json = nlohmann::json;
json Storage::song(const std::string &id) {
  std::lock_guard lock(mutex);
  auto q = prepare("SELECT model,trashed FROM songs WHERE id=?");
  bind(q.get(), 1, id);
  if (sqlite3_step(q.get()) != SQLITE_ROW)
    throw std::invalid_argument("Song not found.");
  auto s = json::parse(
      reinterpret_cast<const char *>(sqlite3_column_text(q.get(), 0)));
  s["trashed"] = sqlite3_column_int(q.get(), 1) != 0;
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
  bool moved = false;
  sql("BEGIN IMMEDIATE");
  try {
    auto q = prepare("INSERT INTO songs(id,title,model) VALUES(?,?,?)");
    bind(q.get(), 1, id);
    bind(q.get(), 2, model.at("title"));
    bind(q.get(), 3, model.dump());
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
  if (!patch.is_object())
    throw std::invalid_argument("Expected song edit object.");
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
  auto update = prepare("UPDATE songs SET title=?,model=? WHERE id=?");
  bind(update.get(), 1, model["title"]);
  bind(update.get(), 2, model.dump());
  bind(update.get(), 3, id);
  if (sqlite3_step(update.get()) != SQLITE_DONE)
    throw std::runtime_error(sqlite3_errmsg(db));
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
    s.erase("notes");
    s.erase("controllers");
    s.erase("rawEvents");
    s["trashed"] = true;
    result.push_back(s);
  }
  return result;
}
} // namespace sightreading
