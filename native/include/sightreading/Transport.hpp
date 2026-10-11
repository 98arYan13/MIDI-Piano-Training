#pragma once
#include "Engine.hpp"
#include "Song.hpp"
#include <map>
#include <set>
#include <tuple>
namespace sightreading {
nlohmann::json inferChords(const nlohmann::json &song);
nlohmann::json songEvents(const nlohmann::json &song, double fromTick,
                          double toTick, unsigned rate, unsigned limit = 4096);
class SongWindow {
public:
  explicit SongWindow(nlohmann::json song);
  nlohmann::json query(double from, double to, unsigned rate, unsigned limit = 4096) const;
  size_t inspected() const { return inspected_; }
  double ppq() const { return metadata_["ppq"].get<double>(); }
private:
  nlohmann::json metadata_, notes_;
  std::vector<size_t> order_;
  std::vector<double> ends_;
  size_t base_{};
  mutable size_t inspected_{};
  void collect(size_t node,size_t left,size_t right,double from,double to,std::vector<size_t> &found,unsigned limit) const;
};
class Transport {
public:
  explicit Transport(Engine &engine) : engine_(engine) {}
  void load(nlohmann::json song);
  nlohmann::json command(const std::string &type, const nlohmann::json &params,
                         uint64_t frame);
  void tick(uint64_t frame);
  void input(const InputEvent &event);
  void recover(uint64_t frame);
  nlohmann::json snapshot(uint64_t frame) const;
  nlohmann::json events(double fromTick, double toTick,
                        unsigned limit = 4096) const;

private:
  Engine &engine_;
  nlohmann::json song_, config_ = {{"speed", 1.0},
                                   {"hearPractice", false},
                                   {"practiceEnabled", true},
                                   {"mode", "continuous"},
                                   {"toleranceMs", 150},
                                   {"spacing", 110},
                                   {"loop", false},
                                   {"a", 0.0},
                                   {"b", 0.0},
                                   {"accompaniment", "Off"},
                                   {"rate", 1.0},
                                   {"accompanimentSpacing", 1.0}};
  std::unique_ptr<SongWindow> window_;
  struct Event {
    double seconds;
    InputEvent input;
    unsigned track;
    uint64_t order{};
  };
  struct Group {
    double seconds, tick;
    std::set<unsigned> pitches;
  };
  std::vector<Event> timeline_;
  std::vector<Group> groups_;
  std::map<std::tuple<unsigned, unsigned, uint64_t>, unsigned> held_;
  std::map<unsigned, uint64_t> attempt_;
  size_t cursor_{}, group_{};
  uint64_t origin_{}, generation_{}, revision_{}, cycle_{};
  uint64_t clockOrigin_{}, scoreCycle_{};
  double clockStart_{}, lastScorePosition_{};
  std::map<std::pair<unsigned, uint64_t>, InputEvent> active_;
  double position_{}, cycleStart_{}, cycleEnd_{};
  bool playing_{}, retry_{};
  bool groupFailed_{};
  unsigned hits_{}, misses_{}, pumpEntries_{};
  std::string error_;
  std::string accompanimentError_, followError_;
  struct InferenceNote {
    double start, end;
    unsigned pc;
  };
  struct RestoreNote {
    double start, end;
    size_t index;
  };
  std::vector<RestoreNote> restoreNotes_;
  std::vector<double> restoreEnds_;
  size_t restoreBase_{};
  std::vector<InferenceNote> inferenceNotes_;
  std::vector<double> inferenceEnds_;
  size_t inferenceBase_{};
  double nextAccompanimentTick_{};
  uint64_t accompanimentInstance_{1000001};
  nlohmann::json cachedChord_;
  struct Control {
    double tick;
    unsigned value;
    int lastUp;
  };
  std::array<std::array<std::vector<Control>, 128>, 16> controls_;
  void accompany(uint64_t frame, double until);
  nlohmann::json barChord(double tick, double &barStart, double &barEnd);
  bool weights(size_t node, size_t left, size_t right, double start, double end,
               std::array<double, 12> &values, unsigned &count) const;
  void build();
  void follow(double seconds, uint64_t frame);
  void cancel();
  void resume(uint64_t frame);
  bool restore(double seconds, uint64_t frame, unsigned budget = 2048);
  double position(uint64_t frame) const;
  double frameSeconds(uint64_t frame) const;
  void fail(uint64_t frame);
  void selectScoreCycle(uint64_t frame);
  bool audible(unsigned track) const;
};
} // namespace sightreading
