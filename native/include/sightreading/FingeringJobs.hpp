#pragma once
#include "FingeringProcess.hpp"
#include "Storage.hpp"
#include <atomic>
#include <functional>
#include <thread>
namespace sightreading {
class FingeringError : public std::runtime_error {
public:
  int status;
  std::string code;
  FingeringError(int s, std::string c, std::string m)
      : runtime_error(m), status(s), code(std::move(c)) {}
};
class FingeringJobs {
public:
  using Clock = std::function<std::chrono::steady_clock::time_point()>;
  FingeringJobs(
      Storage &, std::filesystem::path,
      Clock clock = [] { return std::chrono::steady_clock::now(); });
  ~FingeringJobs();
  nlohmann::json start(const std::string &);
  nlohmann::json snapshot() const;
  void cancel(const std::string &);

private:
  Storage &storage;
  std::filesystem::path root;
  Clock clock;
  mutable std::mutex mutex;
  std::mutex lifecycle;
  nlohmann::json state = {{"state", "idle"},   {"jobId", nullptr},
                          {"songId", nullptr}, {"stage", "idle"},
                          {"completed", 0},    {"total", 0}};
  std::shared_ptr<FingeringProcess> process;
  std::thread worker;
  void run(nlohmann::json request, std::shared_ptr<FingeringProcess> child);
};
} // namespace sightreading
