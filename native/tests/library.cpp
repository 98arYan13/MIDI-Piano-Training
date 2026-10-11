#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <sightreading/Storage.hpp>
using namespace sightreading;
TEST_CASE(
    "Library original and canonical model commit together and trash restores",
    "[library]") {
  auto dir =
      std::filesystem::temp_directory_path() /
      std::filesystem::path(
          "sr-library-" +
          std::to_string(
              std::chrono::steady_clock::now().time_since_epoch().count()));
  nlohmann::json song = {
      {"id", "one"},
      {"title", "Before"},
      {"tracks", nlohmann::json::array({{{"id", 0},
                                         {"practice", true},
                                         {"backing", false},
                                         {"staff", "split"}}})}};
  {
    Storage s(dir);
    s.putSong(song, std::string("MID\0", 4));
    REQUIRE(s.originalSong("one") == std::string("MID\0", 4));
    s.patchSong("one", {{"title", "After"}});
    s.trashSong("one", true);
    REQUIRE(s.songs().empty());
    REQUIRE(s.song("one")["trashed"] == true);
    s.trashSong("one", false);
  }
  {
    Storage s(dir);
    REQUIRE(s.songs().size() == 1);
    REQUIRE(s.song("one")["title"] == "After");
    REQUIRE_THROWS(s.patchSong(
        "one", {{"tracks", nlohmann::json::array({{{"id", 99}}})}}));
    REQUIRE(s.song("one")["tracks"] == song["tracks"]);
  }
  std::filesystem::remove_all(dir);
}
