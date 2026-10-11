#include <catch2/catch_test_macros.hpp>
#include <sfizz/Synth.h>
#include <sfizz/Voice.h>
#include <sfizz/AudioSpan.h>
#include <sfizz/TriggerEvent.h>
#include <array>

static void block(sfz::Synth& s) {
    // Deterministic offline rendering: wait for real streaming workers.
    s.enableFreeWheeling();
    std::array<float,128> left{}, right{};
    float* channels[]{left.data(), right.data()};
    s.renderBlock(sfz::AudioSpan<float>(channels,2,0,128));
}

TEST_CASE("Live pool rejects new voices instead of stealing a held C4") {
    sfz::Synth s;
    s.setSampleRate(48000);
    s.setSamplesPerBlock(128);
    s.setNumVoices(16);
    REQUIRE(s.loadSfzString(SR_ROOT "/assets/piano/test.sfz", "<region> sample=Samples/C4v8.flac note_selfmask=off ampeg_release=10"));
    s.noteOn(0,60,100);
    block(s);
    const auto* original = s.getVoiceView(0);
    REQUIRE(original);
    REQUIRE_FALSE(original->isFree());
    for(int n=0;n<100;n++) { s.noteOn(0,21+n%88,100); block(s); }
    REQUIRE(original->getTriggerEvent().number == 60);
    REQUIRE_FALSE(original->offedOrFree());
}

TEST_CASE("Release tails are not reused by newer attacks") {
    sfz::Synth s;
    s.setSampleRate(48000); s.setSamplesPerBlock(128); s.setNumVoices(4);
    REQUIRE(s.loadSfzString(SR_ROOT "/assets/piano/test.sfz", "<region> sample=Samples/C4v8.flac note_selfmask=off ampeg_release=10"));
    s.noteOn(0,60,100); block(s); s.noteOff(0,60,0); block(s);
    auto* original=s.getVoiceView(0);
    for(int n=0;n<30;n++) { s.noteOn(0,70,100); block(s); }
    REQUIRE(original->getTriggerEvent().number == 60);
}

TEST_CASE("Release-trigger samples retain a reserved allocator slot at attack capacity") {
    sfz::Synth s;
    s.setSampleRate(48000); s.setSamplesPerBlock(128); s.setNumVoices(4);
    REQUIRE(s.loadSfzString(SR_ROOT "/assets/piano/test.sfz", "<global> note_selfmask=off ampeg_release=10 <region> sample=Samples/C4v8.flac <region> trigger=release sample=Samples/C4v8.flac"));
    s.noteOn(0,60,100); block(s);
    s.noteOn(0,61,100); block(s);
    s.noteOn(0,62,100); block(s);
    s.noteOn(0,63,100); block(s);
    REQUIRE(s.getNumActiveVoices() == 3);
    s.noteOff(0,60,0); block(s);
    REQUIRE(s.getNumActiveVoices() == 4);
    REQUIRE(s.getVoiceView(0)->getTriggerEvent().number == 60);
}

TEST_CASE("Same-pitch virtual and native owners release independently") {
    sfz::Synth s;
    s.setSampleRate(48000); s.setSamplesPerBlock(128); s.setNumVoices(16);
    REQUIRE(s.loadSfzString(SR_ROOT "/assets/piano/test.sfz", "<region> sample=Samples/C4v8.flac note_selfmask=off ampeg_release=10"));
    REQUIRE(s.protectedNoteOn(0,60,100,101)); block(s);
    REQUIRE(s.protectedNoteOn(0,60,100,202)); block(s);
    s.protectedNoteOff(0,60,202); block(s);
    REQUIRE_FALSE(s.getVoiceView(0)->released());
    REQUIRE(s.getVoiceView(1)->released());
    s.protectedNoteOff(0,60,101); block(s);
    REQUIRE(s.getVoiceView(0)->released());
}
TEST_CASE("Allocated live capacity preserves held C4 and reports actual allocation refusals") {
    sfz::Synth s;s.setSampleRate(48000);s.setSamplesPerBlock(128);s.setNumVoices(1024);
    REQUIRE(s.loadSfzString(SR_ROOT "/assets/piano/test.sfz", "<global> note_selfmask=off ampeg_release=10 <region> sample=Samples/C4v8.flac <region> trigger=release sample=Samples/C4v8.flac"));
    REQUIRE(s.protectedNoteOn(0,60,100,1));block(s);
    unsigned accepted=1;
    for(uint64_t i=2;i<=1200;++i)if(s.protectedNoteOn(0,64,100,i))++accepted;
    REQUIRE(accepted==960);REQUIRE(s.getNumActiveVoices()==960);REQUIRE(s.getVoiceView(0)->protectedOwner==1);REQUIRE_FALSE(s.getVoiceView(0)->released());
    REQUIRE(s.getProtectedRefusals()==240);
    s.protectedNoteOff(0,60,1);block(s);REQUIRE(s.getNumActiveVoices()==961);REQUIRE(s.getVoiceView(0)->released());
}
TEST_CASE("Partially admitted layered attacks report physical allocation refusals") {
    sfz::Synth s;s.setSampleRate(48000);s.setSamplesPerBlock(128);s.setNumVoices(4);
    REQUIRE(s.loadSfzString(SR_ROOT "/assets/piano/test.sfz", "<global> note_selfmask=off <region> sample=Samples/C4v8.flac <region> sample=Samples/C4v8.flac"));
    REQUIRE(s.protectedNoteOn(0,60,100,1));block(s);
    REQUIRE(s.protectedNoteOn(0,64,100,2));block(s);
    REQUIRE(s.getNumActiveVoices()==3);REQUIRE(s.getProtectedRefusals()==1);
    REQUIRE_FALSE(s.getVoiceView(0)->released());
}
