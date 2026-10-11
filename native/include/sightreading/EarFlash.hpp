#pragma once
#include "Engine.hpp"
#include "Practice.hpp"
namespace sightreading {
json defaultEarConfig();
json defaultFlashConfig();
json validateEarConfig(const json &);
json validateFlashConfig(const json &);
json generateEar(const json &, unsigned seed, unsigned index = 0,
                 bool random = false);
json generateFlash(const json &, unsigned seed);
class EarFlash {
public:
  explicit EarFlash(Engine &engine) : engine_(engine) {}
  json command(const std::string &, const json &, uint64_t);
  void input(const InputEvent &);
  void tick(uint64_t);
  void recoverInput();
  json ear(uint64_t) const;
  json flash() const;

private:
  Engine &engine_;
  json earConfig_ = defaultEarConfig(), flashConfig_ = defaultFlashConfig();
  json earTarget_, flashTarget_, exampleNotes_ = json::array();
  std::vector<InputEvent> schedule_;
  std::map<int, uint64_t> attempt_;
  std::map<std::tuple<unsigned, unsigned, uint64_t>, int> pressed_;
  std::set<unsigned> bent_;
  uint64_t end_{}, pauseRemaining_{}, instance_{}, revision_{},
      flashRevision_{}, elapsedOrigin_{};
  unsigned seed_{1}, index_{}, melodyIndex_{}, hits_{}, misses_{}, flashHits_{},
      flashMisses_{};
  bool playing_{}, paused_{}, autoplay_{}, answered_{}, flashAnswered_{};
  std::string feedback_, flashFeedback_, error_;
  void next(bool, uint64_t);
  void stop();
  void play(const std::string &, uint64_t);
  void evaluate(uint64_t);
};
} // namespace sightreading
