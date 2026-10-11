#pragma once
#include "EventQueue.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <memory>

namespace sightreading {
struct EngineOptions {
    std::filesystem::path pianoPath{"assets/piano/protected.sfz"};
    unsigned sampleRate{48000};
    unsigned blockFrames{128};
    bool exclusive{};
    bool preloadSynchronously{};
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
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
