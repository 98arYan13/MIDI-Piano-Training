#pragma once
#include <sightreading/Engine.hpp>
#include <sightreading/Metronome.hpp>
#include <sfizz/Synth.h>
#include <sfizz/AudioSpan.h>
#include <portaudio.h>
#include <pa_win_wasapi.h>
#include <RtMidi.h>
#include <array>
#include <map>
#include <mutex>
#include <thread>
#include <vector>
#include <tuple>

namespace sightreading {
using json=nlohmann::json;
constexpr unsigned SourceCount=128, NoteCount=2048, BlockFrames=128, ScheduledCount=8192;
struct Engine::Impl {
    explicit Impl(EngineOptions o);
    EngineOptions options; Metronome metronome{options.sampleRate};
    struct PlaybackLane {unsigned volume{100},expression{127},pan{64};std::array<float,BlockFrames> left{},right{};};
    std::array<PlaybackLane,16> playbackLanes{};
    std::array<std::unique_ptr<sfz::Synth>,3> synths;
    EventQueue<> events;
    struct Note {bool active{},held{},sounding{}; InputEvent event{}; uint64_t token{};};
    struct Published {std::atomic<uint64_t> sequence{},packed{},instance{};};
    std::array<Note,NoteCount> notes{};
    std::array<Published,NoteCount> published{};
    std::array<std::array<std::array<bool,16>,SourceCount>,3> pedals{};
    std::array<std::atomic<uint32_t>,SourceCount> sourceGeneration{};
    std::array<std::atomic<uint32_t>,3> busGeneration{};
    std::array<std::atomic<uint64_t>,2> emergencySources{};
    std::atomic<uint32_t> emergencyLiveReset{};std::atomic<bool> liveReconfiguring{};
    std::atomic<bool> emergencyPanic{};std::atomic<uint64_t> emergencyPanicBoundary{};
    struct Scheduled {InputEvent event;uint64_t order;};
    std::array<Scheduled,ScheduledCount> scheduled{};
    unsigned scheduledSize{};uint64_t tokenCounter{},eventOrder{};
    bool renderPianoReadySeen{};
    std::atomic<uint64_t> frame{},overload{},dropouts{},voiceRefusals{};
    std::atomic<unsigned> liveHeld{},livePedals{},actualFrames{};
    std::atomic<bool> pianoReady{},pianoLoading{},audioReady{},running{};
    std::atomic<float> volume{0.7f},velocityCurve{1.f};
    std::atomic<int> outputMode{};
    std::atomic<bool> monitoring{true};std::atomic<uint16_t> channelMask{0xffff};
    RenderHook renderHook{};void* renderContext{};InputHook inputHook{};void* inputContext{};
    mutable std::recursive_mutex control;
    std::mutex externalControl;std::atomic<uint64_t> externalErrors{};
    ExternalMessageHook externalHook{};void* externalContext{};
    std::map<std::tuple<unsigned,unsigned,uint64_t>,uint8_t> externalNotes;
    std::array<std::array<bool,16>,SourceCount> externalPedals{};
    json settings,pending=nullptr,devices=json::array(),outputs=json::array(),audioOutputs=json::array();
    std::string error;std::array<std::string,SourceCount> sourceNames{};unsigned nextSource{1};
    PaStream* stream{};bool paInitialized{};double outputLatency{};unsigned actualRate{};std::string outputName;
    unsigned externalPortOpens{};int openedExternal{-1};std::unique_ptr<RtMidiOut> midiOut;std::thread worker,loader;
    struct InputPort {Impl* engine{};unsigned source{},port{};std::string identity,name;std::unique_ptr<RtMidiIn> input;std::array<std::array<uint64_t,128>,16> instances{};uint64_t counter{};};
    std::map<std::string,std::unique_ptr<InputPort>> ports;std::map<std::string,bool> disabled;
    std::array<std::atomic<uint64_t>,64> monitorMessages{};std::atomic<uint64_t> monitorCount{};
    void loadPiano();unsigned source(const std::string& name);bool submit(InputEvent event);
    void publish(unsigned slot);void off(unsigned slot,int delay);void disconnect(unsigned source,int delay);void panic(uint64_t absolute=UINT64_MAX,uint64_t metronomeBoundary=UINT64_MAX);
    void resetLive(uint64_t absolute) noexcept;
    void laneGain(unsigned channel,unsigned delay) noexcept;
    void dispatch(const InputEvent& event,int delay,uint64_t absolute);void render(float* output,unsigned frames);
    static int callback(const void*,void*,unsigned long,const PaStreamCallbackTimeInfo*,PaStreamCallbackFlags,void*);
    void closeAudio();void openAudio();static void midiCallback(double,std::vector<unsigned char>*,void*);void pollMidi();void applySettings();bool applyPending(bool now);
};
const char* busName(Bus bus);
}
