#pragma once
#include "EventQueue.hpp"
#include "Metronome.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <memory>
#include <vector>

namespace sightreading {
struct EngineOptions {
    std::filesystem::path pianoPath{"assets/piano/protected.sfz"};
    unsigned sampleRate{48000};
    unsigned blockFrames{128};
    bool exclusive{}; // Legacy option, ignored: output always uses shared WASAPI.
    bool preloadSynchronously{};
    std::vector<std::string> disabledInputs{};
};

class Engine {
public:
    explicit Engine(EngineOptions options = {});
    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    nlohmann::json command(const nlohmann::json& request);
    nlohmann::json snapshot() const;
    void start();
    void stop();
    // Interleaved stereo. Call from a single render thread; offline while stopped.
    void render(float* output, unsigned frames);
    bool submit(InputEvent event);
    uint64_t sampleFrame() const noexcept;
    unsigned sampleRate() const noexcept;
    // Register while stopped. The hook adds native bus audio before the limiter.
    using RenderHook = void(*)(float*, unsigned, uint64_t, void*) noexcept;
    void setRenderHook(RenderHook hook, void* context);
    using InputHook = void(*)(const InputEvent&, uint64_t, void*) noexcept;
    void setInputHook(InputHook hook, void* context);
    // Native message sink for embedding/tests; overrides RtMidi delivery while set.
    using ExternalMessageHook = void(*)(const uint8_t*,unsigned,void*) noexcept;
    void setExternalMessageHook(ExternalMessageHook hook,void* context);
    void setBeatHook(Metronome::BeatHook hook,void* context);
    void followMetronome(double tempo,unsigned numerator,unsigned denominator,unsigned beat,uint64_t frame);
private:
    friend struct EngineTestAccess;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
