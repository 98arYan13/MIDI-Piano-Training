#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace sightreading {
std::string resolvedFingeringHand(const nlohmann::json &song,
                                  const nlohmann::json &note);
std::string fingeringFingerprint(const nlohmann::json &song);
nlohmann::json fingeringRequest(const nlohmann::json &song,
                                const std::string &jobId);
void assignFingeringTips(nlohmann::json &song);
class Storage;
}
