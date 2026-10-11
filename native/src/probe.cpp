#include <sightreading/Engine.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>
using namespace sightreading;
static void word(std::ofstream& out,uint32_t value,unsigned bytes){for(unsigned i=0;i<bytes;++i)out.put(char(value>>(i*8)));}
int main(int argc,char** argv){
    try{
        bool live=argc>1&&std::string(argv[1])=="--live";auto begin=std::chrono::steady_clock::now();EngineOptions options;options.preloadSynchronously=!live;Engine engine(options);
        if(live){engine.start();for(unsigned i=0;i<300;++i){auto state=engine.snapshot();if(state["pianoReady"]==true||(!state["pianoLoading"].get<bool>()&&!state["error"].is_null()))break;std::this_thread::sleep_for(std::chrono::milliseconds(100));}auto state=engine.snapshot();state["coldReadyMs"]=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-begin).count();std::cout<<state.dump(2)<<'\n';if(state["audioReady"]==true){engine.submit({EventType::On,Bus::Live,127,0,60,90,1});std::this_thread::sleep_for(std::chrono::milliseconds(500));engine.submit({EventType::Off,Bus::Live,127,0,60,0,1});std::this_thread::sleep_for(std::chrono::seconds(2));std::cout<<engine.snapshot().dump(2)<<'\n';}engine.stop();return state["pianoReady"]==true?0:1;}
        auto state=engine.snapshot();if(state["pianoReady"]!=true){std::cerr<<state.dump()<<'\n';return 1;}
        auto preloadMs=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-begin).count();
        std::vector<float> samples;std::array<float,256> block{};double energy=0,minKeyEnergy=1e9;float peak=0;uint64_t instance=0;
        auto render=[&](unsigned blocks){double local=0;for(unsigned n=0;n<blocks;++n){engine.render(block.data(),128);for(float value:block){if(!std::isfinite(value))throw std::runtime_error("Non-finite piano sample");local+=value*value;peak=std::max(peak,std::abs(value));samples.push_back(value);}}energy+=local;return local;};
        for(unsigned velocity:{20,64,127}){for(unsigned key=21;key<=108;++key){auto id=++instance;engine.submit({EventType::On,Bus::Live,127,0,uint8_t(key),uint8_t(velocity),id});auto keyEnergy=render(30);minKeyEnergy=std::min(minKeyEnergy,keyEnergy);if(keyEnergy<=1e-10)throw std::runtime_error("Silent piano key "+std::to_string(key));engine.submit({EventType::Off,Bus::Live,127,0,uint8_t(key),0,id});render(3);}render(600);}
        engine.submit({EventType::Pedal,Bus::Live,127,0,0,127});engine.submit({EventType::On,Bus::Live,127,0,60,100,++instance});render(100);engine.submit({EventType::Off,Bus::Live,127,0,60,0,instance});render(100);if(engine.snapshot()["heldNotes"].empty())throw std::runtime_error("Pedal failed to retain note");engine.submit({EventType::Pedal,Bus::Live,127,0,0,0});render(600);
        const char* path=argc>1?argv[1]:"build/native/piano-proof.wav";std::ofstream out(path,std::ios::binary);if(!out)throw std::runtime_error("Cannot write WAV");uint32_t size=uint32_t(samples.size()*2);out.write("RIFF",4);word(out,size+36,4);out.write("WAVEfmt ",8);word(out,16,4);word(out,1,2);word(out,2,2);word(out,48000,4);word(out,192000,4);word(out,4,2);word(out,16,2);out.write("data",4);word(out,size,4);for(float value:samples)word(out,uint16_t(int16_t(std::clamp(value,-1.f,1.f)*32767)),2);
        std::cout<<nlohmann::json{{"keys",88},{"velocityLayersChecked",nlohmann::json::array({20,64,127})},{"pedalVerified",true},{"frames",samples.size()/2},{"sampleRate",48000},{"peak",peak},{"rms",std::sqrt(energy/samples.size())},{"minKeyEnergy",minKeyEnergy},{"preloadMs",preloadMs},{"overload",engine.snapshot()["overload"]},{"wav",path}}.dump(2)<<'\n';return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
