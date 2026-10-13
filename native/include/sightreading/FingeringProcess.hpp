#pragma once
#include <chrono>
#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <vector>
namespace sightreading {
class FingeringProcess {
  struct Impl;
  std::unique_ptr<Impl> impl;

public:
  FingeringProcess(const std::filesystem::path &,
                   const std::vector<std::wstring> &);
  ~FingeringProcess();
  void writeFrame(const nlohmann::json &);
  std::optional<nlohmann::json> readFrame(std::chrono::milliseconds);
  void terminate();
  std::string diagnostics() const;
};
} // namespace sightreading
