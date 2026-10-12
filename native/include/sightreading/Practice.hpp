#pragma once
#include "EventQueue.hpp"
#include <map>
#include <nlohmann/json.hpp>
#include <random>
#include <set>
#include <tuple>
#include <vector>
namespace sightreading {
using json = nlohmann::json;
struct SpelledPitch {
  int midi{};
  char letter{'c'};
  int accidental{}, octave{};
  json notation() const;
};
std::vector<SpelledPitch> scalePitches(const std::string &key,
                                       const std::string &scale, int low,
                                       int high);
struct PracticeTarget {
  std::vector<SpelledPitch> pitches;
  std::string symbol;
  int fingering{};
  json jsonValue() const;
};
json defaultPracticeConfig();
json validatePracticeConfig(const json &config);
std::vector<PracticeTarget> generatePractice(const json &config, unsigned seed,
                                             unsigned count = 64);
// Control-worker-only. Audio hooks enqueue InputEvent; never call this class
// there.
class Practice {
public:
  explicit Practice(unsigned sampleRate = 48000) : rate_(sampleRate) {}
  json configure(const json &patch);
  void start(uint64_t frame);
  void stop(uint64_t frame);
  void pause(uint64_t frame);
  void resume(uint64_t frame);
  void reset(uint64_t frame);
  void input(const InputEvent &event);
  void tick(uint64_t frame);
  json snapshot(uint64_t frame) const;
  uint64_t revision() const noexcept { return revision_; }
  void recoverHeld(const json &heldNotes, uint64_t frame);

private:
  unsigned rate_;
  json config_ = defaultPracticeConfig();
  std::vector<PracticeTarget> targets_;
  std::map<std::tuple<unsigned, unsigned, uint64_t>, int> pressed_;
  std::map<int, uint64_t> attempt_;
  std::vector<std::string> results_;
  uint64_t origin_{}, stoppedFrame_{}, scoringFrom_{}, revision_{};
  unsigned index_{}, hits_{}, misses_{};
  bool running_{}, paused_{};
  std::string error_;
  uint64_t targetFrame(unsigned index) const;
  void miss();
  void hit(uint64_t frame);
  void evaluate(uint64_t frame);
};
} // namespace sightreading
