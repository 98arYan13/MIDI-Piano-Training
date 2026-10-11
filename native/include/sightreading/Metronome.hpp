#pragma once
#include <array>
#include <atomic>
#include <nlohmann/json.hpp>
namespace sightreading {
struct MetronomeConfig {
 double tempo{100},swing{50}; unsigned numerator{4},denominator{4},subdivision{1},countIn{1},sound{}; float volume{0.7f};bool mute{},follow{};
 std::array<unsigned,12> accents{2,1,1,1,1,1,1,1,1,1,1,1};
};
struct BeatEvent {uint64_t sampleFrame{},bar{};unsigned beat{},subdivision{};bool countIn{};};
class Metronome {
public:
 using BeatHook=void(*)(const BeatEvent&,void*) noexcept;
 explicit Metronome(unsigned rate);
 nlohmann::json command(const std::string&,const nlohmann::json&);
 nlohmann::json snapshot()const;
 void follow(double tempo,unsigned numerator,unsigned denominator,unsigned beat,uint64_t frame);
 void render(float*,unsigned,uint64_t) noexcept;
 uint64_t boundary()const noexcept{return write.load(std::memory_order_acquire);}
 void panic(uint64_t boundary=UINT64_MAX)noexcept;
 BeatHook hook{};void* context{};
private:
 struct Change {MetronomeConfig config;unsigned operation{},beat{};uint64_t frame{},sequence{};};
 bool push(Change);std::array<Change,64> queue{};std::atomic<uint64_t> read{},write{};
 std::array<Change,64> scheduled{};unsigned scheduledSize{};std::atomic<unsigned> followOutstanding{};uint64_t canceled{};
 MetronomeConfig requested{},active{};unsigned rate; bool running{},pending{};MetronomeConfig target{};
 double next{},beatStart{}; unsigned beat{},part{};uint64_t bar{};unsigned countBars{};
 struct Voice {unsigned sound{},position{};float gain{};bool active{};};std::array<Voice,16> voices{};
 std::array<std::array<float,2400>,3> sounds{};
 std::atomic<bool> publishedRunning{};std::atomic<double> publishedTempo{100};std::atomic<unsigned> publishedBeat{},publishedDenominator{4},publishedNumerator{4};std::atomic<uint64_t> publishedBar{};
};
}
