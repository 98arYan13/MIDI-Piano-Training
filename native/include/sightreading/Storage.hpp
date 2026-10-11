#pragma once
#include <filesystem>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <sqlite3.h>

namespace sightreading {
// Control-thread only. A statement commit is atomic; migrations use a
// transaction.
class Storage {
  sqlite3 *db{};
  std::mutex mutex;
  using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;
  Statement prepare(const char *text) {
    sqlite3_stmt *statement{};
    if (sqlite3_prepare_v2(db, text, -1, &statement, nullptr) != SQLITE_OK)
      throw std::runtime_error(sqlite3_errmsg(db));
    return Statement(statement, sqlite3_finalize);
  }
  void sql(const char *text) {
    char *error{};
    if (sqlite3_exec(db, text, nullptr, nullptr, &error) != SQLITE_OK) {
      std::string message = error ? error : "SQLite failure";
      sqlite3_free(error);
      throw std::runtime_error(message);
    }
  }
  void bind(sqlite3_stmt *statement, int index, const std::string &value) {
    if (sqlite3_bind_text(statement, index, value.c_str(), -1,
                          SQLITE_TRANSIENT) != SQLITE_OK)
      throw std::runtime_error(sqlite3_errmsg(db));
  }

public:
  explicit Storage(const std::filesystem::path &dir) {
    std::filesystem::create_directories(dir);
    auto path = (dir / "practice.sqlite").u8string();
    if (sqlite3_open(reinterpret_cast<const char *>(path.c_str()), &db) !=
        SQLITE_OK) {
      sqlite3_close(db);
      db = nullptr;
      throw std::runtime_error("Cannot open local database");
    }
    try {
      sqlite3_busy_timeout(db, 3000);
      {
        auto statement = prepare("PRAGMA user_version");
        if (sqlite3_step(statement.get()) != SQLITE_ROW)
          throw std::runtime_error("Cannot inspect database schema");
        if (sqlite3_column_int(statement.get(), 0) > 1)
          throw std::runtime_error(
              "Database schema is newer than this application");
      }
      sql("PRAGMA journal_mode=WAL; BEGIN; CREATE TABLE IF NOT EXISTS "
          "settings(key TEXT PRIMARY KEY,value TEXT NOT NULL); CREATE TABLE IF "
          "NOT EXISTS songs(id TEXT PRIMARY KEY,title TEXT NOT NULL,model TEXT "
          "NOT NULL,trashed INTEGER NOT NULL DEFAULT 0); PRAGMA "
          "user_version=1; COMMIT;");
    } catch (...) {
      sqlite3_close(db);
      db = nullptr;
      throw;
    }
  }
  ~Storage() { sqlite3_close(db); }
  Storage(const Storage &) = delete;
  Storage &operator=(const Storage &) = delete;
  nlohmann::json load(const std::string &key) {
    std::lock_guard lock(mutex);
    auto statement = prepare("SELECT value FROM settings WHERE key=?");
    bind(statement.get(), 1, key);
    auto code = sqlite3_step(statement.get());
    if (code == SQLITE_DONE)
      return nullptr;
    if (code != SQLITE_ROW)
      throw std::runtime_error(sqlite3_errmsg(db));
    return nlohmann::json::parse(reinterpret_cast<const char *>(
        sqlite3_column_text(statement.get(), 0)));
  }
  void save(const std::string &key, const nlohmann::json &value) {
    std::lock_guard lock(mutex);
    auto statement =
        prepare("INSERT INTO settings(key,value) VALUES(?,?) ON CONFLICT(key) "
                "DO UPDATE SET value=excluded.value");
    bind(statement.get(), 1, key);
    bind(statement.get(), 2, value.dump());
    if (sqlite3_step(statement.get()) != SQLITE_DONE)
      throw std::runtime_error(sqlite3_errmsg(db));
  }
  nlohmann::json songs() {
    std::lock_guard lock(mutex);
    auto statement =
        prepare("SELECT id,title FROM songs WHERE trashed=0 ORDER BY title");
    auto result = nlohmann::json::array();
    int code;
    while ((code = sqlite3_step(statement.get())) == SQLITE_ROW)
      result.push_back(
          {{"id", reinterpret_cast<const char *>(
                      sqlite3_column_text(statement.get(), 0))},
           {"title", reinterpret_cast<const char *>(
                         sqlite3_column_text(statement.get(), 1))}});
    if (code != SQLITE_DONE)
      throw std::runtime_error(sqlite3_errmsg(db));
    return result;
  }
};
} // namespace sightreading
