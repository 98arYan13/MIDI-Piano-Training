#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <sightreading/Storage.hpp>
using namespace sightreading;
TEST_CASE("SQLite settings survive restart with complete nested values") {
  auto dir =
      std::filesystem::temp_directory_path() /
      std::filesystem::path(
          "sr-storage-" +
          std::to_string(
              std::chrono::steady_clock::now().time_since_epoch().count()));
  nlohmann::json settings = {{"tempo", 121},
                             {"meter", {{"numerator", 6}, {"denominator", 8}}},
                             {"accents", {"strong", "weak", "mute"}}};
  {
    Storage s(dir);
    s.save("metro", settings);
    REQUIRE(s.load("missing").is_null());
    REQUIRE(s.songs().empty());
  }
  {
    Storage s(dir);
    REQUIRE(s.load("metro") == settings);
    s.save("metro", {{"tempo", 150}});
    REQUIRE(s.load("metro")["tempo"] == 150);
  }
  std::filesystem::remove_all(dir);
}
TEST_CASE("Newer SQLite schema is rejected instead of downgraded") {
  auto dir =
      std::filesystem::temp_directory_path() /
      std::filesystem::path(
          "sr-future-" +
          std::to_string(
              std::chrono::steady_clock::now().time_since_epoch().count()));
  {
    Storage s(dir);
  }
  sqlite3 *db{};
  sqlite3_open((dir / "practice.sqlite").string().c_str(), &db);
  sqlite3_exec(db, "PRAGMA user_version=2", nullptr, nullptr, nullptr);
  sqlite3_close(db);
  REQUIRE_THROWS_AS(Storage(dir), std::runtime_error);
  std::filesystem::remove_all(dir);
}
