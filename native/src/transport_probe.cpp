#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>

#include <sightreading/Transport.hpp>
#include <windows.h>
#include <psapi.h>
using namespace sightreading;
using json = nlohmann::json;
struct Capture {
  std::array<InputEvent, 4096> notes{};
  size_t size{};
  std::array<BeatEvent, 64> beats{};
  size_t beatSize{};
};
static void word(std::ofstream &out, uint32_t value, unsigned bytes) {
  for (unsigned i = 0; i < bytes; ++i)
    out.put(char(value >> (i * 8)));
}
int main(int argc, char **argv) {
  try {
    auto start = std::chrono::steady_clock::now();
    EngineOptions options;
    options.preloadSynchronously = true;
    Engine engine(options);
    if (engine.snapshot()["pianoReady"] != true)
      throw std::runtime_error("Sampled piano not ready.");
    auto preloadMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now() - start)
                         .count();
    std::ifstream file("assets/music/twinkle.mid", std::ios::binary);
    std::string bytes((std::istreambuf_iterator<char>(file)), {});
    auto song = importMidi(bytes, "Timing proof");
    song["id"] = "proof";
    Capture capture;
    engine.setInputHook(
        [](const InputEvent &e, uint64_t frame, void *context) noexcept {
          auto &c = *static_cast<Capture *>(context);
          if (c.size < c.notes.size()) {
            c.notes[c.size] = e;
            c.notes[c.size++].frame = frame;
          }
        },
        &capture);
    engine.setBeatHook(
        [](const BeatEvent &e, void *context) noexcept {
          auto &c = *static_cast<Capture *>(context);
          if (c.beatSize < c.beats.size())
            c.beats[c.beatSize++] = e;
        },
        &capture);
    Transport transport(engine);
    transport.load(song);
    transport.command("transport.configure",
                      {{"hearPractice", true}, {"practiceEnabled", false}}, 0);
    engine.command({{"version", 1},
                    {"id", "metro"},
                    {"type", "metronome.config"},
                    {"params", {{"tempo", 100}, {"countIn", 0}}}});
    engine.command({{"version", 1},
                    {"id", "metro-start"},
                    {"type", "metronome.start"},
                    {"params", json::object()}});
    engine.submit({EventType::On, Bus::Live, 1, 0, 76, 90, 999});
    transport.command("transport.play", {}, 0);
    std::vector<float> samples(48000 * 4 * 2);
    double energy = 0;
    float peak = 0;
    auto renderedAt = std::chrono::steady_clock::now();
    for (unsigned frame = 0; frame < 48000 * 4; frame += 128) {
      if (frame == 96000)
        transport.command("transport.stop", {}, frame);
      transport.tick(frame);
      engine.render(samples.data() + frame * 2, 128);
    }
    auto renderMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - renderedAt)
                        .count();
    for (float sample : samples) {
      if (!std::isfinite(sample))
        throw std::runtime_error("Nonfinite WAV.");
      energy += sample * sample;
      peak = std::max(peak, std::abs(sample));
    }
    if (energy < .01 || peak >= 1)
      throw std::runtime_error("Invalid sampled audio energy/limiter.");
    unsigned onCount = 0;
    json timing = json::array();
    for (size_t i = 0; i < capture.size; i++) {
      auto &e = capture.notes[i];
      if (e.bus != Bus::Playback || e.type != EventType::On ||
          e.source != SongSource)
        continue;
      auto note = std::find_if(song["notes"].begin(), song["notes"].end(),
                               [&](auto &n) { return n["id"] == e.instance; });
      uint64_t expected =
          256 + uint64_t(std::llround(tickSeconds(song, (*note)["startTick"]) *
                                      48000));
      if (e.frame != expected)
        throw std::runtime_error("Playback sample-frame timing differs.");
      if (e.frame >= 96000)
        throw std::runtime_error("Canceled generation sounded after Stop.");
      timing.push_back(
          {{"frame", e.frame}, {"expected", expected}, {"note", e.note}});
      ++onCount;
    }
    if (onCount < 3 || capture.beatSize < 4 ||
        capture.beats[0].sampleFrame != 0 ||
        capture.beats[1].sampleFrame != 28800 ||
        capture.beats[2].sampleFrame != 57600)
      throw std::runtime_error("Missing exact native song/click timing.");
    auto state = engine.snapshot();
    if (!state["metronome"]["running"].get<bool>() ||
        state["heldNotes"].size() != 1 ||
        state["heldNotes"][0]["instance"] != 999)
      throw std::runtime_error("Stop affected live/click ownership.");
    const std::string path =
        argc > 1 ? argv[1] : "build/native/transport-proof.wav";
    std::ofstream out(path, std::ios::binary);
    uint32_t size = uint32_t(samples.size() * 2);
    out.write("RIFF", 4);
    word(out, size + 36, 4);
    out.write("WAVEfmt ", 8);
    word(out, 16, 4);
    word(out, 1, 2);
    word(out, 2, 2);
    word(out, 48000, 4);
    word(out, 192000, 4);
    word(out, 4, 2);
    word(out, 16, 2);
    out.write("data", 4);
    word(out, size, 4);
    for (float sample : samples)
      word(out, uint16_t(int16_t(std::clamp(sample, -1.f, 1.f) * 32767)), 2);
    if (!out)
      throw std::runtime_error("WAV write failed.");
    PROCESS_MEMORY_COUNTERS memory{};
    GetProcessMemoryInfo(GetCurrentProcess(), &memory, sizeof(memory));
    std::cout << json{{"preloadMs", preloadMs},
                      {"renderMs", renderMs},
                      {"renderedSeconds", 4},
                      {"workingSetBytes", memory.WorkingSetSize},
                      {"peakWorkingSetBytes", memory.PeakWorkingSetSize},
                      {"peak", peak},
                      {"rms", std::sqrt(energy / samples.size())},
                      {"exactPlaybackFrames", timing},
                      {"clickFrames", json::array({0, 28800, 57600})},
                      {"stopPreservedLiveAndClicks", true},
                      {"wav", path}}
                     .dump(2)
              << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
