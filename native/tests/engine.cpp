#include <catch2/catch_test_macros.hpp>
#include <sightreading/Engine.hpp>
#include <sightreading/EarFlash.hpp>
#include <array>
#include <cmath>
#include <thread>
#include <chrono>
#include <portaudio.h>
#include <sfizz/Synth.h>
#include <sfizz/AudioSpan.h>
#include "../src/EngineInternal.hpp"
namespace sightreading {
struct EngineTestAccess {
 static void abortAudio(Engine& e) {
  std::lock_guard lock(e.impl_->control);
  REQUIRE(Pa_AbortStream(e.impl_->stream) == paNoError);
 }
 static void changeCachedOutputIndex(Engine& e) {
  std::lock_guard lock(e.impl_->control);
  e.impl_->settings["outputDevice"]=999;
  e.impl_->openedOutputDevice=999;
 }
 static void selectVanishedOutput(Engine& e) {
  std::lock_guard lock(e.impl_->control);
  e.impl_->openedOutputDevice=-2;
  for(auto& d:e.impl_->audioOutputs)
   if(d["id"]==e.impl_->settings["outputDevice"])d["name"]="Disconnected test endpoint";
 }
};
}
using namespace sightreading;
using nlohmann::json;
static void fillFuture(Engine& e, Bus bus=Bus::Example, unsigned source=125) {
 for(unsigned batch=0;batch<2;++batch){for(unsigned i=0;i<4096;++i){InputEvent ev{EventType::Pedal,bus,static_cast<uint16_t>(source),0,0,0,0};ev.frame=32768;REQUIRE(e.submit(ev));}std::array<float,256> out{};e.render(out.data(),128);}
}
static EngineOptions fixture(){return {SR_ROOT "/native/tests/fixture.sfz",48000,128,false,true};}
static json cmd(Engine& e,const char* type,json params={}){return e.command({{"version",1},{"id",1},{"type",type},{"params",params}});}
static double audio(Engine& e,unsigned blocks=1){std::array<float,256> out{}; double sum=0; for(unsigned i=0;i<blocks;i++){e.render(out.data(),128); for(float f:out){REQUIRE(std::isfinite(f)); sum+=f*f;}}return sum;}
TEST_CASE("Stopped WASAPI stream recovers without resetting live notes or clicks", "[audio-recovery]") {
 Engine e(fixture()); e.start();
 if(!e.snapshot()["audioReady"].get<bool>()) { WARN("No WASAPI output for recovery hardware test"); e.stop(); return; }
 REQUIRE(cmd(e,"metronome.start")["ok"] == true);
 REQUIRE(e.submit({EventType::On,Bus::Live,1,0,60,90,777}));
 std::this_thread::sleep_for(std::chrono::milliseconds(80));
 REQUIRE(e.snapshot()["heldNotes"].size() == 1);
 EngineTestAccess::abortAudio(e);
 const auto stopped = e.sampleFrame();
 bool recovered = false;
 for(unsigned i=0;i<40;++i) {
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  if(e.sampleFrame()>stopped+128) { recovered=true; break; }
 }
 REQUIRE(recovered);
 const auto state=e.snapshot();
 REQUIRE(state["audioReady"] == true);
 REQUIRE(state["heldNotes"].size() == 1);
 REQUIRE(state["heldNotes"][0]["instance"] == 777);
 REQUIRE(state["metronome"]["running"] == true);
 // An explicitly selected device retains its name when enumeration indices change.
 auto selected=state["audioOutputs"][0];
 for(const auto& d:state["audioOutputs"])if(d["name"]==state["output"]["name"])selected=d;
 REQUIRE(e.submit({EventType::Off,Bus::Live,1,0,60,0,777}));
 std::this_thread::sleep_for(std::chrono::milliseconds(80));
 REQUIRE(cmd(e,"audio.settings",{{"outputDevice",selected["id"]}})["ok"] == true);
 EngineTestAccess::changeCachedOutputIndex(e);
 EngineTestAccess::abortAudio(e);
 const auto beforeRemap=e.sampleFrame();
 for(unsigned i=0;i<40&&e.sampleFrame()<=beforeRemap+128;++i)
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
 REQUIRE(e.sampleFrame()>beforeRemap+128);
 REQUIRE(e.snapshot()["output"]["name"]==selected["name"]);
 REQUIRE(e.snapshot()["settings"]["outputDevice"]==selected["id"]);
 EngineTestAccess::selectVanishedOutput(e);
 EngineTestAccess::abortAudio(e);
 std::this_thread::sleep_for(std::chrono::milliseconds(1600));
 REQUIRE(e.snapshot()["audioReady"]==false);
 REQUIRE(e.snapshot()["error"].get<std::string>().find("Disconnected test endpoint")!=std::string::npos);
 REQUIRE(cmd(e,"audio.settings",{{"outputDevice",-1}})["ok"] == true);
 e.stop();
}

TEST_CASE("Maximum piano volume retains sample level with bounded output", "[piano-gain]") {
 for(auto bus:{Bus::Live,Bus::Playback,Bus::Example}) {
  Engine e(fixture());REQUIRE(cmd(e,"audio.settings",{{"volume",1}})["ok"]==true);
  sfz::Synth reference;reference.setSampleRate(48000);reference.setSamplesPerBlock(128);
  reference.enableFreeWheeling();REQUIRE(reference.loadSfzFile(SR_ROOT "/native/tests/fixture.sfz"));
  reference.noteOn(0,60,100);
  REQUIRE(e.submit({EventType::On,bus,static_cast<uint16_t>(bus==Bus::Playback?126:127),0,60,100,1}));
  std::array<float,128> left{},right{};std::array<float,256> actual{};float* channels[]{left.data(),right.data()};
  double expectedEnergy=0,actualEnergy=0;
  for(unsigned block=0;block<30;++block){
   reference.renderBlock(sfz::AudioSpan<float>(channels,2,0,128));e.render(actual.data(),128);
   const float lane=bus==Bus::Playback?100.f/127.f:1.f;
   for(unsigned i=0;i<128;++i)for(unsigned ch=0;ch<2;++ch){
    const float x=channels[ch][i]*lane;const float expected=x/(1.f+std::abs(x));
    expectedEnergy+=expected*expected;actualEnergy+=actual[i*2+ch]*actual[i*2+ch];
    REQUIRE(std::isfinite(actual[i*2+ch]));REQUIRE(std::abs(actual[i*2+ch])<1.f);
   }
  }
  INFO("bus="<<unsigned(bus)<<" actual="<<actualEnergy<<" expected="<<expectedEnergy);
  REQUIRE(actualEnergy>expectedEnergy*0.9);REQUIRE(actualEnergy<expectedEnergy*1.1);
  REQUIRE(cmd(e,"audio.settings",{{"volume",0}})["ok"]==true);REQUIRE(audio(e,4)==0);
 }
}

TEST_CASE("Legacy exclusive settings cannot reserve system audio", "[audio-sharing]") {
 auto options=fixture();options.exclusive=true;Engine e(options);
 REQUIRE(e.snapshot()["settings"]["exclusive"]==false);
 REQUIRE(cmd(e,"audio.settings",{{"exclusive",true},{"volume",1}})["ok"]==true);
 REQUIRE(e.snapshot()["settings"]["exclusive"]==false);
 REQUIRE(e.snapshot()["settings"]["volume"]==1);
 REQUIRE(e.snapshot()["output"]["exclusive"]==false);
}

TEST_CASE("Piano readiness requires genuine complete sampled instrument") {
    Engine e(fixture()); REQUIRE(e.snapshot()["pianoReady"]==true); REQUIRE(e.snapshot()["audioReady"]==false);
    Engine missing({SR_ROOT "/native/tests/missing.sfz",48000,128,false,true}); REQUIRE(missing.snapshot()["pianoReady"]==false); REQUIRE_FALSE(missing.snapshot()["error"].get<std::string>().empty());
}
TEST_CASE("Full future schedule retains Panic and scoped stop cancellation", "[final-control]") {
 for(auto type:{EventType::Panic,EventType::StopBus})for(auto bus:{Bus::Playback,Bus::Example}){
  Engine e(fixture());cmd(e,"metronome.start");
  REQUIRE(e.submit({EventType::On,Bus::Live,124,0,60,100,1}));
  REQUIRE(e.submit({EventType::On,Bus::Playback,126,0,64,100,2}));
  REQUIRE(e.submit({EventType::On,Bus::Example,125,0,67,100,3}));audio(e);
  fillFuture(e,bus==Bus::Example?Bus::Playback:Bus::Example,bus==Bus::Example?126:125);
  REQUIRE(e.submit({type,bus}));audio(e);
  auto notes=e.snapshot()["heldNotes"];
  if(type==EventType::Panic){REQUIRE(notes.empty());REQUIRE(e.snapshot()["metronome"]["running"]==false);}
  else {REQUIRE(notes.size()==2);for(auto& n:notes)REQUIRE(n["bus"]!=(bus==Bus::Playback?"playback":"example"));REQUIRE(e.snapshot()["metronome"]["running"]==true);}
 }
}
TEST_CASE("Full future schedule Panic preserves later metronome Start and attack", "[final-control]") {
 Engine e(fixture());cmd(e,"metronome.start");audio(e);fillFuture(e);
 REQUIRE(e.submit({EventType::Panic}));cmd(e,"metronome.start");
 REQUIRE(e.submit({EventType::On,Bus::Live,124,0,60,100,42}));audio(e);
 REQUIRE(e.snapshot()["metronome"]["running"]==true);REQUIRE(e.snapshot()["heldNotes"].size()==1);
 REQUIRE(e.snapshot()["heldNotes"][0]["instance"]==42);
}
TEST_CASE("Reserved scope controls ignore unrelated default-source release recovery", "[final-control]") {
 for(auto type:{EventType::Panic,EventType::StopBus,EventType::ResetLive}){
  Engine e(fixture());cmd(e,"metronome.start");
  REQUIRE(e.submit({EventType::On,Bus::Live,124,0,60,90,1}));REQUIRE(e.submit({EventType::On,Bus::Playback,126,0,64,90,2}));audio(e);fillFuture(e);
  REQUIRE(e.submit({EventType::Off,Bus::Live,0,0,60,0,99}));
  REQUIRE(e.submit({type,type==EventType::StopBus?Bus::Playback:Bus::Live}));audio(e);
  auto notes=e.snapshot()["heldNotes"];
  if(type==EventType::Panic){REQUIRE(notes.empty());REQUIRE(e.snapshot()["metronome"]["running"]==false);}
  else {REQUIRE(notes.size()==1);REQUIRE(notes[0]["bus"]==(type==EventType::StopBus?"live":"playback"));REQUIRE(e.snapshot()["metronome"]["running"]==true);}
 }
}
TEST_CASE("Full producer queue reserves only requested stop bus", "[final-control]") {
 for(auto bus:{Bus::Playback,Bus::Example}){
  Engine e(fixture());cmd(e,"metronome.start");
  REQUIRE(e.submit({EventType::On,Bus::Live,124,0,60,90,1}));REQUIRE(e.submit({EventType::On,Bus::Playback,126,0,64,90,2}));REQUIRE(e.submit({EventType::On,Bus::Example,125,0,67,90,3}));audio(e);
  for(unsigned i=0;i<4096;i++)REQUIRE(e.submit({EventType::Pedal,Bus::Live,123,0,0,0,0}));
  REQUIRE(e.submit({EventType::StopBus,bus}));audio(e);auto notes=e.snapshot()["heldNotes"];
  REQUIRE(notes.size()==2);for(auto& n:notes)REQUIRE(n["bus"]!=(bus==Bus::Playback?"playback":"example"));REQUIRE(e.snapshot()["metronome"]["running"]==true);
 }
}
TEST_CASE("Named source exhaustion preserves existing owners and reusable identities", "[final-registry]") {
 Engine e(fixture());cmd(e,"metronome.start");
 for(unsigned i=1;i<=124;++i)REQUIRE(cmd(e,"virtual.noteOn",{{"source","slot:"+std::to_string(i)},{"note",60},{"velocity",80},{"instance",i}})["ok"]==true);
 audio(e);auto before=e.snapshot()["heldNotes"];
 auto failed=cmd(e,"virtual.noteOn",{{"source","one-too-many"},{"note",64},{"velocity",80},{"instance",125}});
 REQUIRE(failed["ok"]==false);REQUIRE(failed["error"]["message"].get<std::string>().find("Restart")!=std::string::npos);
 REQUIRE(cmd(e,"virtual.noteOff",{{"source","slot:1"},{"note",60},{"instance",1}})["ok"]==true);audio(e);
 REQUIRE(e.snapshot()["heldNotes"].size()==before.size()-1);REQUIRE(e.snapshot()["metronome"]["running"]==true);
}
TEST_CASE("Ear config invalid integer replacement is atomic", "[final-validation]") {
 Engine e(fixture());EarFlash controller(e);auto before=controller.ear(0)["config"];
 REQUIRE_THROWS(controller.command("ear.configure",{{"root",uint64_t{4294967356}}},0));
 REQUIRE(controller.ear(0)["config"]==before);
}
TEST_CASE("Virtual disconnect and source off preserve hardware same pitch") {
    Engine e(fixture());
    REQUIRE(e.submit({EventType::On,Bus::Live,127,0,60,100,9}));
    REQUIRE(cmd(e,"virtual.noteOn",{{"source","browser:test"},{"instance",1},{"note",60},{"velocity",100}})["ok"]==true);
    REQUIRE(audio(e,16)>0.0001); REQUIRE(e.snapshot()["heldNotes"].size()==2);
    REQUIRE(cmd(e,"virtual.disconnect",{{"source","browser:test"}})["ok"]==true);
    audio(e); REQUIRE(e.snapshot()["heldNotes"].size()==1); REQUIRE(e.snapshot()["heldNotes"][0]["sourceId"]==127);
    REQUIRE(audio(e,64)>0.0001);
}
TEST_CASE("Playback stop and pedal changes cannot release live keys") {
    Engine e(fixture());
    REQUIRE(e.submit({EventType::On,Bus::Live,127,0,60,100,9}));
    REQUIRE(e.submit({EventType::On,Bus::Playback,126,0,60,100,1})); audio(e);
    REQUIRE(cmd(e,"bus.stop",{{"bus","playback"}})["ok"]==true); audio(e);
    REQUIRE(e.snapshot()["heldNotes"].size()==1); REQUIRE(e.snapshot()["heldNotes"][0]["bus"]=="live");
}
TEST_CASE("Browser pedal up and disconnect cannot release physical sustained voices") {
    Engine e(fixture());
    REQUIRE(e.submit({EventType::Pedal,Bus::Live,127,0,0,127,0}));
    REQUIRE(e.submit({EventType::On,Bus::Live,127,0,60,100,9}));
    REQUIRE(e.submit({EventType::Off,Bus::Live,127,0,60,0,9}));
    REQUIRE(cmd(e,"virtual.pedal",{{"source","browser:test"},{"down",true}})["ok"]==true);
    REQUIRE(cmd(e,"virtual.noteOn",{{"source","browser:test"},{"instance",1},{"note",60},{"velocity",100}})["ok"]==true);
    REQUIRE(cmd(e,"virtual.noteOff",{{"source","browser:test"},{"instance",1}})["ok"]==true); audio(e);
    REQUIRE(e.snapshot()["heldNotes"].size()==2);
    REQUIRE(cmd(e,"virtual.pedal",{{"source","browser:test"},{"down",false}})["ok"]==true); audio(e);
    REQUIRE(e.snapshot()["heldNotes"].size()==1); REQUIRE(e.snapshot()["heldNotes"][0]["sourceId"]==127); REQUIRE(e.snapshot()["heldNotes"][0]["held"]==false);
    cmd(e,"virtual.disconnect",{{"source","browser:test"}}); audio(e,64);
    REQUIRE(e.snapshot()["heldNotes"].size()==1);
    REQUIRE(e.submit({EventType::Pedal,Bus::Live,127,0,0,0,0})); audio(e); REQUIRE(e.snapshot()["heldNotes"].empty());
}
TEST_CASE("Pedal holds source notes and defers disruptive settings until release") {
    Engine e(fixture());
    REQUIRE(cmd(e,"virtual.pedal",{{"source","browser:test"},{"down",true}})["ok"]==true);
    REQUIRE(cmd(e,"virtual.noteOn",{{"source","browser:test"},{"instance",1},{"note",60},{"velocity",100}})["ok"]==true); audio(e);
    REQUIRE(cmd(e,"virtual.noteOff",{{"source","browser:test"},{"instance",1}})["ok"]==true); audio(e);
    REQUIRE(e.snapshot()["heldNotes"].size()==1); REQUIRE(e.snapshot()["heldNotes"][0]["held"]==false);
    REQUIRE(cmd(e,"audio.settings",{{"outputMode","silent"}})["ok"]==true); REQUIRE_FALSE(e.snapshot()["pendingChange"].is_null());
    REQUIRE(cmd(e,"virtual.pedal",{{"source","browser:test"},{"down",false}})["ok"]==true); audio(e);
    REQUIRE(e.snapshot()["heldNotes"].empty());
    cmd(e,"audio.applyPending"); REQUIRE(e.snapshot()["pendingChange"].is_null()); REQUIRE(e.snapshot()["settings"]["outputMode"]=="silent");
}
TEST_CASE("Versioned commands reject malformed parameters without allocating notes") {
    Engine e(fixture());
    REQUIRE_FALSE(cmd(e,"virtual.noteOn",{{"source","browser:test"},{"instance",1},{"note",999},{"velocity",100}})["ok"].get<bool>());
    REQUIRE_FALSE(e.command({{"version",2},{"id",2},{"type","panic"},{"params",json::object()}})["ok"].get<bool>());
    REQUIRE(cmd(e,"unknown")["error"]["code"]=="unknown_command"); REQUIRE(e.snapshot()["heldNotes"].empty());
}
TEST_CASE("Stopping playback cancels scheduled future attacks and keeps the native hook running") {
    Engine e(fixture());
    unsigned hookCalls=0;
    e.setRenderHook([](float*,unsigned,uint64_t,void* p) noexcept{++*static_cast<unsigned*>(p);},&hookCalls);
    InputEvent future{EventType::On,Bus::Playback,126,0,60,100,1,500}; REQUIRE(e.submit(future));
    REQUIRE(e.submit({EventType::On,Bus::Live,127,0,60,100,1}));
    REQUIRE(cmd(e,"bus.stop",{{"bus","playback"}})["ok"]==true);
    audio(e,8); REQUIRE(hookCalls==8); REQUIRE(e.snapshot()["heldNotes"].size()==1); REQUIRE(e.snapshot()["heldNotes"][0]["bus"]=="live");
}
TEST_CASE("Events use the native sample frame precisely") {
    std::array<uint64_t,2> observed{};Engine e(fixture());
    e.setInputHook([](const InputEvent& event,uint64_t frame,void* p) noexcept{auto* times=static_cast<uint64_t*>(p);times[event.type==EventType::On?0:1]=frame;},observed.data());
    REQUIRE(e.submit({EventType::On,Bus::Live,127,0,60,100,1,37}));
    REQUIRE(e.submit({EventType::Off,Bus::Live,127,0,60,0,1,211}));audio(e,2);
    REQUIRE(observed[0]==37);REQUIRE(observed[1]==211);REQUIRE(e.sampleFrame()==256);REQUIRE(e.snapshot()["heldNotes"].empty());
}
TEST_CASE("Explicit Panic notifies the native global hook and clears notes") {
    bool stopped=false;Engine e(fixture());
    e.setInputHook([](const InputEvent& event,uint64_t,void* p) noexcept{if(event.type==EventType::Panic)*static_cast<bool*>(p)=true;},&stopped);
    REQUIRE(e.submit({EventType::On,Bus::Live,127,0,60,100,1}));audio(e);
    REQUIRE(cmd(e,"panic")["ok"]==true);audio(e);
    REQUIRE(stopped);REQUIRE(e.snapshot()["heldNotes"].empty());
}
TEST_CASE("Native sample clock can run while a missing piano is reported not ready") {
    REQUIRE(Pa_Initialize()==paNoError);auto host=Pa_HostApiTypeIdToHostApiIndex(paWASAPI);bool available=host>=0&&Pa_GetHostApiInfo(host)->defaultOutputDevice>=0;Pa_Terminate();
    Engine e({SR_ROOT "/native/tests/missing.sfz"});e.start();
    for(unsigned i=0;i<100&&e.snapshot()["pianoLoading"]==true;++i)std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto state=e.snapshot();REQUIRE(state["pianoReady"]==false);
    if(available) {REQUIRE(state["audioReady"]==true);std::this_thread::sleep_for(std::chrono::milliseconds(40));REQUIRE(e.sampleFrame()>0);}
    e.stop();
}
TEST_CASE("Release overflow recovers only the pressured source") {
    Engine e(fixture()); REQUIRE(e.submit({EventType::On,Bus::Live,127,0,60,100,1}));audio(e);
    for(unsigned i=0;i<3072;++i)REQUIRE(e.submit({EventType::On,Bus::Live,1,0,64,100,i+1}));
    for(unsigned i=0;i<1024;++i)REQUIRE(e.submit({EventType::Off,Bus::Live,1,0,64,0,i+1}));
    REQUIRE_FALSE(e.submit({EventType::Off,Bus::Live,1,0,64,0,1}));audio(e);
    REQUIRE(e.snapshot()["heldNotes"].size()==1);REQUIRE(e.snapshot()["heldNotes"][0]["sourceId"]==127);REQUIRE(e.snapshot()["heldNotes"][0]["held"]==true);REQUIRE(e.snapshot()["heldNotes"][0]["sounding"]==true);
}
struct ExternalMessages {
    std::array<std::array<uint8_t,3>,128> messages{};unsigned count{};
    static void capture(const uint8_t* bytes,unsigned size,void* context) noexcept {
        auto& sink=*static_cast<ExternalMessages*>(context);
        if(size==3&&sink.count<sink.messages.size())sink.messages[sink.count++]={bytes[0],bytes[1],bytes[2]};
    }
};
TEST_CASE("External monitoring blocks attacks and pedal down but retains releases", "[routing]") {
    ExternalMessages sink;Engine e(fixture());e.setExternalMessageHook(ExternalMessages::capture,&sink);
    REQUIRE(cmd(e,"audio.settings",{{"outputMode","external"},{"monitoring",false}})["ok"]==true);
    REQUIRE(e.submit({EventType::On,Bus::Live,127,0,60,100,1}));
    REQUIRE(e.submit({EventType::Pedal,Bus::Live,127,0,0,127}));
    REQUIRE(sink.count==0);
    REQUIRE(e.submit({EventType::Pedal,Bus::Live,127,0,0,0}));
    REQUIRE(sink.count==1);REQUIRE(sink.messages[0]==std::array<uint8_t,3>{0xb0,64,0});
    REQUIRE(e.submit({EventType::Off,Bus::Live,127,0,60,0,1}));REQUIRE(sink.count==1);
}
TEST_CASE("External owners release after monitoring becomes disabled", "[routing]") {
    ExternalMessages sink;Engine e(fixture());e.setExternalMessageHook(ExternalMessages::capture,&sink);
    REQUIRE(cmd(e,"audio.settings",{{"outputMode","external"}})["ok"]==true);
    REQUIRE(e.submit({EventType::On,Bus::Live,127,0,60,100,1}));
    REQUIRE(e.submit({EventType::Pedal,Bus::Live,127,0,0,127}));
    REQUIRE(sink.count==2);REQUIRE(sink.messages[0]==std::array<uint8_t,3>{0x90,60,100});
    // Native events remain queued here; exercise the producer routing transition directly.
    REQUIRE(cmd(e,"audio.settings",{{"monitoring",false}})["ok"]==true);
    REQUIRE(e.submit({EventType::On,Bus::Live,127,0,64,100,2}));
    REQUIRE(e.submit({EventType::Pedal,Bus::Live,126,0,0,127}));
    REQUIRE(sink.count==2);
    REQUIRE(e.submit({EventType::Off,Bus::Live,127,0,60,0,1}));
    REQUIRE(sink.messages[2]==std::array<uint8_t,3>{0x80,60,0});
    REQUIRE(e.submit({EventType::Pedal,Bus::Live,127,0,0,0}));
    REQUIRE(sink.messages[3]==std::array<uint8_t,3>{0xb0,64,0});
}
TEST_CASE("Virtual commands cannot claim a reserved physical MIDI source name", "[routing]") {
    Engine e(fixture());REQUIRE(e.submit({EventType::Pedal,Bus::Live,127,0,0,127}));
    REQUIRE(e.submit({EventType::On,Bus::Live,127,0,60,100,1}));
    REQUIRE(e.submit({EventType::Off,Bus::Live,127,0,60,0,1}));audio(e);
    for(auto type:{"virtual.disconnect","virtual.pedal","virtual.noteOn","virtual.noteOff"}) {
        auto reply=cmd(e,type,{{"source","midi:fixture keyboard#0"},{"down",false},{"note",60},{"velocity",100},{"instance",1}});
        REQUIRE(reply["ok"]==false);REQUIRE(reply["error"]["code"]=="invalid_params");
    }
    audio(e);REQUIRE(e.snapshot()["heldNotes"].size()==1);
    REQUIRE(e.snapshot()["heldNotes"][0]["sourceId"]==127);REQUIRE(e.snapshot()["heldNotes"][0]["sustained"]==true);
}
TEST_CASE("Saved disabled MIDI identities exist before native discovery") {
 auto options=fixture();options.disabledInputs={"Saved Keyboard#0"};Engine e(options);
 REQUIRE(e.snapshot().contains("disabledInputs"));REQUIRE(e.snapshot()["disabledInputs"]["Saved Keyboard#0"]==true);
}
TEST_CASE("Apply now clears live ownership without clearing other buses or metronome") {
 Engine e(fixture());cmd(e,"metronome.config",{{"countIn",0}});cmd(e,"metronome.start");
 REQUIRE(e.submit({EventType::On,Bus::Live,127,0,60,100,1}));
 REQUIRE(e.submit({EventType::Pedal,Bus::Live,127,0,0,127,0}));
 REQUIRE(e.submit({EventType::On,Bus::Playback,126,0,64,100,2}));
 REQUIRE(e.submit({EventType::On,Bus::Example,125,0,67,100,3}));
 InputEvent future{EventType::On,Bus::Playback,126,0,72,100,4};future.frame=512;REQUIRE(e.submit(future));audio(e);
 auto frame=e.sampleFrame();REQUIRE(cmd(e,"audio.settings",{{"outputMode","silent"}})["result"]["deferred"]==true);
 REQUIRE(cmd(e,"audio.applyPending",{{"applyNow",true}})["result"]["applied"]==true);
 REQUIRE(e.snapshot()["metronome"]["running"]==true);
 audio(e);auto preserved=e.snapshot()["heldNotes"];REQUIRE(preserved.size()==2);for(auto& n:preserved)REQUIRE(n["bus"]!="live");
 audio(e,8);REQUIRE(e.sampleFrame()>frame);REQUIRE(e.snapshot()["heldNotes"].size()==3);REQUIRE(e.snapshot()["metronome"]["running"]==true);
}

TEST_CASE("Apply now has reserved live recovery under a full event queue") {
 Engine e(fixture());cmd(e,"metronome.config",{{"countIn",0}});cmd(e,"metronome.start");
 REQUIRE(e.submit({EventType::On,Bus::Live,127,0,60,100,1}));REQUIRE(e.submit({EventType::On,Bus::Playback,126,0,64,100,2}));audio(e);
 InputEvent stale{EventType::On,Bus::Live,127,0,72,100,3};stale.frame=2048;REQUIRE(e.submit(stale));
 for(unsigned i=0;i<4095;i++)REQUIRE(e.submit({EventType::Pedal,Bus::Example,125,0,0,0,0}));
 REQUIRE(cmd(e,"audio.settings",{{"monitoring",false}})["result"]["deferred"]==true);
 REQUIRE(cmd(e,"audio.applyPending",{{"applyNow",true}})["ok"]==true);audio(e,20);
 auto notes=e.snapshot()["heldNotes"];REQUIRE(notes.size()==1);REQUIRE(notes[0]["bus"]=="playback");REQUIRE(e.snapshot()["metronome"]["running"]==true);
}
TEST_CASE("Apply now releases external live notes and sustain") {
 struct Capture{std::array<std::array<uint8_t,3>,128> messages{};unsigned count{};} capture;
 Engine e(fixture());e.setExternalMessageHook([](const uint8_t* bytes,unsigned size,void* ptr)noexcept{auto& c=*static_cast<Capture*>(ptr);if(size==3&&c.count<c.messages.size()){std::copy_n(bytes,3,c.messages[c.count++].begin());}},&capture);
 cmd(e,"audio.settings",{{"outputMode","external"}});REQUIRE(e.submit({EventType::On,Bus::Live,127,0,60,100,1}));REQUIRE(e.submit({EventType::Pedal,Bus::Live,127,0,0,127,0}));audio(e);
 REQUIRE(cmd(e,"audio.settings",{{"outputMode","silent"}})["result"]["deferred"]==true);cmd(e,"audio.applyPending",{{"applyNow",true}});audio(e);
 bool off=false,pedalUp=false;for(unsigned i=0;i<capture.count;i++){auto m=capture.messages[i];off|=m[0]==0x80&&m[1]==60;pedalUp|=m[0]==0xb0&&m[1]==64&&m[2]==0;}
 REQUIRE(off);REQUIRE(pedalUp);REQUIRE(e.snapshot()["heldNotes"].empty());
}
TEST_CASE("Rapid metronome field edits merge canonical native requested state") {
 Engine e(fixture());REQUIRE(cmd(e,"metronome.meter",{{"denominator",8}})["ok"]==true);REQUIRE(cmd(e,"metronome.meter",{{"numerator",6}})["ok"]==true);
 REQUIRE(cmd(e,"metronome.accent",{{"beat",1},{"accent","mute"}})["ok"]==true);REQUIRE(cmd(e,"metronome.accent",{{"beat",2},{"accent","strong"}})["ok"]==true);
 auto settings=e.snapshot()["metronome"]["settings"];REQUIRE(settings["meter"]["denominator"]==8);REQUIRE(settings["meter"]["numerator"]==6);REQUIRE(settings["accents"][1]=="mute");REQUIRE(settings["accents"][2]=="strong");
 REQUIRE(cmd(e,"metronome.meter",{{"numerator",13}})["ok"]==false);REQUIRE(cmd(e,"metronome.meter",{{"denominator",3}})["ok"]==false);REQUIRE(cmd(e,"metronome.meter",{{"unknown",1}})["ok"]==false);REQUIRE(cmd(e,"metronome.accent",{{"beat",6},{"accent","mute"}})["ok"]==false);REQUIRE(cmd(e,"metronome.accent",{{"beat",1},{"accent","bad"}})["ok"]==false);
 REQUIRE(e.snapshot()["metronome"]["settings"]==settings);
}
TEST_CASE("Rapid audio channel edits merge native settings without lost channels") {
 Engine e(fixture());REQUIRE(cmd(e,"audio.channel",{{"channel",0},{"enabled",false}})["ok"]==true);REQUIRE(cmd(e,"audio.channel",{{"channel",1},{"enabled",false}})["ok"]==true);
 auto channels=e.snapshot()["settings"]["channels"];REQUIRE(channels.size()==14);for(auto& channel:channels)REQUIRE((channel!=0&&channel!=1));
 REQUIRE(cmd(e,"audio.channel",{{"channel",16},{"enabled",false}})["ok"]==false);REQUIRE(cmd(e,"audio.channel",{{"channel",1},{"enabled","bad"}})["ok"]==false);REQUIRE(cmd(e,"audio.channel",{{"channel",1},{"enabled",true},{"unknown",true}})["ok"]==false);REQUIRE(e.snapshot()["settings"]["channels"]==channels);
}
TEST_CASE("Apply now recovers live ownership with a full non-live future scheduler") {
 unsigned futurePedals=0;
 Engine e(fixture());e.setInputHook([](const InputEvent& ev,uint64_t,void* ptr)noexcept{if(ev.type==EventType::Pedal&&ev.bus==Bus::Example)++*static_cast<unsigned*>(ptr);},&futurePedals);cmd(e,"metronome.config",{{"countIn",0}});cmd(e,"metronome.start");
 REQUIRE(e.submit({EventType::On,Bus::Live,127,0,60,100,1}));REQUIRE(e.submit({EventType::Pedal,Bus::Live,127,0,0,127,0}));REQUIRE(e.submit({EventType::On,Bus::Playback,126,0,64,100,2}));audio(e);
 for(unsigned batch=0;batch<2;batch++){for(unsigned i=0;i<4096;i++){InputEvent future{EventType::Pedal,Bus::Example,125,0,0,0,0};future.frame=16384;REQUIRE(e.submit(future));}audio(e);}
 REQUIRE(cmd(e,"audio.settings",{{"monitoring",false}})["result"]["deferred"]==true);REQUIRE(cmd(e,"audio.applyPending",{{"applyNow",true}})["ok"]==true);audio(e);
 auto notes=e.snapshot()["heldNotes"];REQUIRE(notes.size()==1);REQUIRE(notes[0]["bus"]=="playback");REQUIRE(e.snapshot()["metronome"]["running"]==true);
 REQUIRE(cmd(e,"audio.settings",{{"monitoring",true}})["result"]["deferred"]==false);
 audio(e,140);REQUIRE(futurePedals==8192);REQUIRE(e.sampleFrame()>16384);REQUIRE(e.submit({EventType::On,Bus::Live,127,0,67,100,3}));audio(e);REQUIRE(e.snapshot()["heldNotes"].size()==2);
}
