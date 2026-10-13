#pragma once
#include <nlohmann/json.hpp>
#include <set>
#include <stdexcept>

namespace sightreading {
inline nlohmann::json defaultPreferences() {
  return {{"route", "staff"}, {"sidebarHidden", false}, {"focusMode", false}, {"fingeringTips", "medium"},
          {"keyboard", {{"hidden", false}, {"focused", false}, {"size", 88},
                        {"labels", true}, {"velocity", 90}, {"octave", 4}}}};
}
inline nlohmann::json updatePreferences(nlohmann::json current,
                                        const nlohmann::json &patch) {
  if (!patch.is_object()) throw std::invalid_argument("Preferences must be an object.");
  if (!current.contains("fingeringTips")) current["fingeringTips"] = "medium";
  for (auto it = patch.begin(); it != patch.end(); ++it) {
    if (!current.contains(it.key())) throw std::invalid_argument("Unknown preference.");
    if (it.key() == "keyboard") {
      if (!it.value().is_object()) throw std::invalid_argument("Keyboard preferences must be an object.");
      for (auto k = it.value().begin(); k != it.value().end(); ++k) {
        if (!current["keyboard"].contains(k.key())) throw std::invalid_argument("Unknown keyboard preference.");
        current["keyboard"][k.key()] = k.value();
      }
    } else current[it.key()] = it.value();
  }
  const std::set<std::string> routes{"staff", "ear", "flash", "play", "guide"};
  const std::set<std::string> densities{"full", "medium", "pro"};
  if (!current["fingeringTips"].is_string() || !densities.contains(current["fingeringTips"].get<std::string>()))
    throw std::invalid_argument("Fingering tips must be full, medium or pro.");
  if (!current["route"].is_string() || !routes.contains(current["route"].get<std::string>()))
    throw std::invalid_argument("Unknown practice tab.");
  for (auto key : {"sidebarHidden", "focusMode"})
    if (!current[key].is_boolean()) throw std::invalid_argument("Display switches must be boolean.");
  auto &keyboard = current["keyboard"];
  for (auto key : {"hidden", "focused", "labels"})
    if (!keyboard[key].is_boolean()) throw std::invalid_argument("Keyboard switches must be boolean.");
  const std::set<int> sizes{25, 32, 37, 49, 61, 76, 88};
  if (!keyboard["size"].is_number_integer() || !sizes.contains(keyboard["size"].get<int>()))
    throw std::invalid_argument("Unknown keyboard size.");
  for (auto [key, maximum] : {std::pair{"velocity", 127}, std::pair{"octave", 7}})
    if (!keyboard[key].is_number_integer() || keyboard[key].get<int>() < 1 || keyboard[key].get<int>() > maximum)
      throw std::invalid_argument("Keyboard value out of range.");
  return current;
}
}
