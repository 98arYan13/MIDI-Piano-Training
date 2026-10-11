#include <catch2/catch_test_macros.hpp>
#include <sightreading/Engine.hpp>
#include <vector>
using namespace sightreading;
TEST_CASE("Global metronome renders exact native quarter beats without piano", "[metronome]") {
 Engine e({"missing.sfz",48000,128,false,true});
 auto command=[&](const char* t,nlohmann::json p){return e.command({{"version",1},{"id",1},{"type",t},{"params",p}});};
 REQUIRE(command("metronome.config",{{"countIn",0}})["ok"]==true);
 REQUIRE(command("metronome.start",{})["ok"]==true);
 std::vector<float> out(60000*2); e.render(out.data(),60000);
 REQUIRE(out[0]!=0); REQUIRE(out[28800*2]!=0); REQUIRE(out[57600*2]!=0);
 REQUIRE(out[20000*2]==0);
 REQUIRE(e.snapshot()["metronome"]["running"]==true);
 REQUIRE(command("metronome.stop",{})["ok"]==true);e.render(out.data(),60000);
 for(float f:out)REQUIRE(f==0);
}
struct Beats {std::array<BeatEvent,128> events{};unsigned size{};static void receive(const BeatEvent& b,void* p)noexcept{auto& s=*static_cast<Beats*>(p);if(s.size<s.events.size())s.events[s.size++]=b;}};
static nlohmann::json metroCommand(Engine& e,const char* t,nlohmann::json p=nlohmann::json::object()){return e.command({{"version",1},{"id",1},{"type",t},{"params",p}});}
static void renderFrames(Engine& e,unsigned n){std::vector<float> out(n*2);e.render(out.data(),n);}
TEST_CASE("Metronome subdivision swing mute and count-in use sample clock","[metronome]"){
 for(auto pair:{std::pair{"eighth",2u},std::pair{"triplet",3u},std::pair{"sixteenth",4u}}){
 Engine e({"missing.sfz",48000,128,false,true});Beats beats;e.setBeatHook(Beats::receive,&beats);
 REQUIRE(metroCommand(e,"metronome.config",{{"subdivision",pair.first},{"mute",true}})["ok"]==true);metroCommand(e,"metronome.start");std::vector<float> out(115201*2);e.render(out.data(),115201);
 REQUIRE(beats.size==4*pair.second+1);REQUIRE(beats.events[1].sampleFrame==28800/pair.second);REQUIRE(beats.events[0].countIn);REQUIRE_FALSE(beats.events[4*pair.second].countIn);
 REQUIRE(std::all_of(out.begin(),out.end(),[](float f){return f==0;}));
 }
 Engine e({"missing.sfz",48000,128,false,true});Beats beats;e.setBeatHook(Beats::receive,&beats);metroCommand(e,"metronome.config",{{"subdivision","eighth"},{"swing",75}});metroCommand(e,"metronome.start");renderFrames(e,28801);REQUIRE(beats.events[1].sampleFrame==21600);REQUIRE(beats.events[2].sampleFrame==28800);
}
TEST_CASE("Metronome edits land next beat and next bar without resetting frame","[metronome]"){
 Engine e({"missing.sfz",48000,128,false,true});Beats b;e.setBeatHook(Beats::receive,&b);metroCommand(e,"metronome.start");renderFrames(e,1000);
 REQUIRE(metroCommand(e,"metronome.config",{{"tempo",200},{"meter",{{"numerator",3},{"denominator",8}}},{"subdivision","eighth"}})["ok"]==true);
 renderFrames(e,71101);REQUIRE(b.events[1].sampleFrame==28800);REQUIRE(b.events[2].sampleFrame==43200);REQUIRE(b.events[3].sampleFrame==57600);REQUIRE(b.events[4].sampleFrame==72000);REQUIRE(b.events[4].beat==0);REQUIRE(e.snapshot()["metronome"]["activeMeter"]["denominator"]==8);
 renderFrames(e,3600);REQUIRE(b.events[5].sampleFrame==75600);REQUIRE(e.sampleFrame()==75701);
}
TEST_CASE("Follow and piano bus stops leave global click running","[metronome]"){
 Engine e({SR_ROOT "/native/tests/fixture.sfz",48000,128,false,true});Beats b;e.setBeatHook(Beats::receive,&b);
 metroCommand(e,"metronome.config",{{"mode","follow"},{"countIn",0}});metroCommand(e,"metronome.start");renderFrames(e,128);e.followMetronome(120,3,4,1,1000);renderFrames(e,25000);REQUIRE(b.events[1].sampleFrame==1000);REQUIRE(b.events[1].beat==1);REQUIRE(b.events[2].sampleFrame==25000);
 REQUIRE(e.submit({EventType::On,Bus::Live,127,0,60,100,1}));metroCommand(e,"bus.stop",{{"bus","playback"}});renderFrames(e,128);REQUIRE(e.snapshot()["metronome"]["running"]==true);REQUIRE(e.snapshot()["heldNotes"].size()==1);
 metroCommand(e,"metronome.stop");std::vector<float> out(256);e.render(out.data(),128);REQUIRE(e.snapshot()["heldNotes"].size()==1);REQUIRE(std::any_of(out.begin(),out.end(),[](float f){return f!=0;}));
 metroCommand(e,"metronome.start");metroCommand(e,"panic");renderFrames(e,128);REQUIRE(e.snapshot()["metronome"]["running"]==false);REQUIRE(e.snapshot()["heldNotes"].empty());
}
TEST_CASE("Metronome validates controls and settings exclude running","[metronome]"){
 Engine e({"missing.sfz",48000,128,false,true});for(auto p:{nlohmann::json{{"tempo",19}},nlohmann::json{{"swing",76}},nlohmann::json{{"countIn",5}},nlohmann::json{{"meter",{{"numerator",13},{"denominator",4}}}}})REQUIRE(metroCommand(e,"metronome.config",p)["ok"]==false);
 for(auto sound:{"click","woodblock","rim"}){REQUIRE(metroCommand(e,"metronome.config",{{"sound",sound},{"accents",{"mute","mute","mute","mute"}}})["ok"]==true);metroCommand(e,"metronome.start");std::vector<float> out(256);e.render(out.data(),128);REQUIRE(std::all_of(out.begin(),out.end(),[](float f){return f==0;}));metroCommand(e,"metronome.stop");renderFrames(e,128);}
 REQUIRE_FALSE(e.snapshot()["metronome"]["settings"].contains("running"));
}
TEST_CASE("Original click sounds coexist with sampled live piano and retain bus ownership","[metronome]"){
 for(auto sound:{"click","woodblock","rim"}){
 Engine piano({SR_ROOT "/native/tests/fixture.sfz",48000,128,false,true});Engine mixed({SR_ROOT "/native/tests/fixture.sfz",48000,128,false,true});
 REQUIRE(piano.submit({EventType::On,Bus::Live,127,0,60,100,1}));REQUIRE(mixed.submit({EventType::On,Bus::Live,127,0,60,100,1}));
 metroCommand(mixed,"metronome.config",{{"sound",sound},{"countIn",0}});metroCommand(mixed,"metronome.start");
 std::vector<float> a(256),b(256);piano.render(a.data(),128);mixed.render(b.data(),128);REQUIRE(a[0]!=b[0]);REQUIRE(mixed.snapshot()["heldNotes"].size()==1);
 metroCommand(mixed,"bus.stop",{{"bus","example"}});renderFrames(mixed,128);REQUIRE(mixed.snapshot()["metronome"]["running"]==true);
 }
}
TEST_CASE("Panic cancellation respects later metronome commands","[metronome]"){
 Engine e({"missing.sfz",48000,128,false,true});metroCommand(e,"panic");metroCommand(e,"metronome.start");renderFrames(e,128);REQUIRE(e.snapshot()["metronome"]["running"]==true);
 metroCommand(e,"metronome.start");metroCommand(e,"panic");renderFrames(e,128);REQUIRE(e.snapshot()["metronome"]["running"]==false);
}
TEST_CASE("Metronome settings restore directly","[metronome]"){
 Engine a({"missing.sfz",48000,128,false,true}),b({"missing.sfz",48000,128,false,true});auto settings=a.snapshot()["metronome"]["settings"];REQUIRE(metroCommand(b,"metronome.config",settings)["ok"]==true);REQUIRE(b.snapshot()["metronome"]["settings"]==settings);
}
TEST_CASE("Future Follow changes retain intervening beats and ordered positions","[metronome]"){
 Engine e({"missing.sfz",48000,128,false,true});Beats b;e.setBeatHook(Beats::receive,&b);metroCommand(e,"metronome.config",{{"mode","follow"}});metroCommand(e,"metronome.start");renderFrames(e,128);e.followMetronome(120,4,4,0,60000);e.followMetronome(200,3,4,1,100000);renderFrames(e,110000);
 REQUIRE(b.events[1].sampleFrame==28800);REQUIRE(b.events[2].sampleFrame==57600);REQUIRE(b.events[3].sampleFrame==60000);REQUIRE(b.events[4].sampleFrame==84000);REQUIRE(b.events[5].sampleFrame==100000);REQUIRE(b.events[5].beat==1);
}
TEST_CASE("Follow reservations refuse overflow and release on Panic","[metronome]"){
 Engine e({"missing.sfz",48000,128,false,true});metroCommand(e,"metronome.config",{{"mode","follow"}});metroCommand(e,"metronome.start");renderFrames(e,128);
 for(unsigned i=0;i<63;++i)e.followMetronome(100,4,4,i%4,100000+i*100);REQUIRE_THROWS(e.followMetronome(100,4,4,0,200000));renderFrames(e,128);REQUIRE_THROWS(e.followMetronome(100,4,4,0,200000));
 metroCommand(e,"panic");renderFrames(e,128);REQUIRE_NOTHROW(e.followMetronome(100,4,4,0,200000));
}
TEST_CASE("Follow same-frame changes apply in producer order and restore validates beat unit","[metronome]"){
 Engine e({"missing.sfz",48000,128,false,true});Beats b;e.setBeatHook(Beats::receive,&b);metroCommand(e,"metronome.config",{{"mode","follow"}});metroCommand(e,"metronome.start");renderFrames(e,128);e.followMetronome(120,4,4,1,1000);e.followMetronome(100,4,4,2,1000);renderFrames(e,1000);REQUIRE(b.events[1].beat==2);
 REQUIRE(metroCommand(e,"metronome.config",{{"beatUnit",8}})["ok"]==false);
}
