#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>

namespace sightreading {
enum class Bus : uint8_t { Live, Playback, Example, Click };
enum class EventType : uint8_t { On, Off, Pedal, Disconnect, StopBus, Panic, ResetLive };
struct InputEvent {
    EventType type{};
    Bus bus{Bus::Live};
    uint16_t source{};
    uint8_t channel{}, note{}, velocity{};
    uint64_t instance{}, frame{};
    uint32_t generation{};
    uint32_t busGeneration{};
};

template<size_t Capacity=4096, size_t ReleaseReserve=1024>
class EventQueue {
public:
    // Producers serialize here; the audio consumer never acquires this lock.
    bool push(const InputEvent& event) {
        std::lock_guard guard(producers_);
        auto write = write_.load(std::memory_order_relaxed);
        auto used = write-read_.load(std::memory_order_acquire);
        const auto limit = event.type==EventType::On ? Capacity-ReleaseReserve : Capacity;
        if(used >= limit) return false;
        events_[write%Capacity] = event;
        write_.store(write+1,std::memory_order_release);
        return true;
    }
    bool pop(InputEvent& event) noexcept {
        const auto read=read_.load(std::memory_order_relaxed);
        if(read==write_.load(std::memory_order_acquire)) return false;
        event=events_[read%Capacity];
        read_.store(read+1,std::memory_order_release);
        return true;
    }
private:
    static_assert(ReleaseReserve<Capacity);
    std::array<InputEvent,Capacity> events_{};
    alignas(64) std::atomic<uint64_t> read_{};
    alignas(64) std::atomic<uint64_t> write_{};
    std::mutex producers_;
};
}
