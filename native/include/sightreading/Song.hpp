#pragma once
#include <nlohmann/json.hpp>
#include <string>
namespace sightreading {
nlohmann::json importMidi(const std::string &bytes, const std::string &title);
double tickSeconds(const nlohmann::json &song, double tick);
double secondsTick(const nlohmann::json &song, double seconds);
void prepareSongTiming(nlohmann::json &song);
} // namespace sightreading
