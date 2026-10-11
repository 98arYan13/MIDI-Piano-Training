#include "EngineInternal.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <regex>

static_assert(sfz::config::maxVoices == 1024, "Run setup-native: protected sfizz capacity patch required");
namespace sightreading {
const char* busName(Bus b){return b==Bus::Live?"live":b==Bus::Playback?"playback":b==Bus::Example?"example":"click";}
Engine::Impl::Impl(EngineOptions o):options(std::move(o)),settings({{"volume",0.7},{"monitoring",true},{"outputMode","internal"},{"velocityCurve",1.0},{"channels",json::array({0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15})},{"outputDevice",-1},{"exclusive",options.exclusive},{"externalDevice",-1}}){
    if(options.sampleRate<8000||options.sampleRate>192000)throw std::invalid_argument("Unsupported sample rate");
    if(options.blockFrames!=128)throw std::invalid_argument("Initial native block request must be 128 frames");
    if(options.preloadSynchronously)loadPiano();
}
void Engine::Impl::loadPiano(){
    pianoLoading=true;
    try{
        std::ifstream file(options.pianoPath);if(!file)throw std::runtime_error("Piano SFZ missing. Run scripts/setup-native.ps1.");
        std::string text((std::istreambuf_iterator<char>(file)),{});
        std::regex samples("sample=([^\\r\\n ]+)"),defaultPath("default_path=([^\\r\\n ]+)");std::smatch base;
        auto folder=options.pianoPath.parent_path();if(std::regex_search(text,base,defaultPath))folder/=base[1].str();
        unsigned count=0;
        for(std::sregex_iterator i(text.begin(),text.end(),samples),end;i!=end;++i){auto name=(*i)[1].str();if(name.starts_with('*'))throw std::runtime_error("Synthetic piano regions are prohibited");auto path=folder/name;if(!std::filesystem::is_regular_file(path)||std::filesystem::file_size(path)<128)throw std::runtime_error("Piano sample missing/corrupt: "+name+". Run scripts/setup-native.ps1.");++count;}
        if(!count)throw std::runtime_error("Piano SFZ contains no sampled regions");
        for(unsigned i=0;i<3;++i){auto& pointer=synths[i];pointer=std::make_unique<sfz::Synth>();auto& s=*pointer;s.setSampleRate(float(options.sampleRate));s.setSamplesPerBlock(BlockFrames);s.setNumVoices(i==0?1024:256);s.setPreloadSize(16384);s.enableFreeWheeling();if(!s.loadSfzFile(options.pianoPath.string())||s.getNumRegions()!=int(count)||!s.getNumPreloadedSamples())throw std::runtime_error("Piano decoding/preload failed. Verify pinned assets.");}
        if(running.load())for(auto& s:synths)s->disableFreeWheeling();
        pianoReady.store(true,std::memory_order_release);
    }catch(const std::exception& e){std::lock_guard lock(control);error=e.what();pianoReady=false;}
    pianoLoading=false;
}
unsigned Engine::Impl::source(const std::string& name){
    if(name.empty()||name.size()>160)throw std::invalid_argument("source must have 1-160 characters");
    for(unsigned i=1;i<nextSource;++i)if(sourceNames[i]==name)return i;
    if(nextSource>=SourceCount-2)throw std::runtime_error("Source limit reached");sourceNames[nextSource]=name;return nextSource++;
}
bool Engine::Impl::submit(InputEvent e){
    if(e.source>=SourceCount||e.channel>15||static_cast<unsigned>(e.bus)>2)return false;
    if(e.type==EventType::On&&(e.note<21||e.note>108||e.velocity==0||e.velocity>127||!e.instance))return false;
    if(e.type==EventType::StopBus&&e.bus==Bus::Live)return false;
    if(e.type==EventType::On&&e.bus==Bus::Live&&!(channelMask.load()&(1u<<e.channel)))return true;
    if(e.type==EventType::Disconnect)e.generation=sourceGeneration[e.source].fetch_add(1)+1;else e.generation=sourceGeneration[e.source].load();
    e.busGeneration=e.type==EventType::StopBus?busGeneration[unsigned(e.bus)].fetch_add(1)+1:busGeneration[unsigned(e.bus)].load();
    if(outputMode==1&&e.bus==Bus::Live){
        std::lock_guard lock(externalControl);
        auto send=[&](unsigned status,unsigned note,unsigned value){unsigned char bytes[3]{uint8_t(status),uint8_t(note),uint8_t(value)};if(externalHook){externalHook(bytes,3,externalContext);return;}if(midiOut&&midiOut->isPortOpen()){try{midiOut->sendMessage(bytes,3);}catch(const RtMidiError&){++externalErrors;}}};
        auto release=[&](auto iterator){auto [source,channel,instance]=iterator->first;auto note=iterator->second;externalNotes.erase(iterator);bool other=false;for(auto& [key,pitch]:externalNotes)if(std::get<1>(key)==channel&&pitch==note)other=true;if(!other)send(0x80|channel,note,0);};
        auto key=std::tuple<unsigned,unsigned,uint64_t>{e.source,e.channel,e.instance};
        if(e.type==EventType::On&&monitoring.load()){externalNotes[key]=e.note;send(0x90|e.channel,e.note,e.velocity);}
        if(e.type==EventType::Off){auto i=externalNotes.find(key);if(i!=externalNotes.end())release(i);}
        if(e.type==EventType::Pedal&&(e.velocity<64||monitoring.load())){externalPedals[e.source][e.channel]=e.velocity>=64;bool down=false;for(auto& source:externalPedals)down|=source[e.channel];send(0xb0|e.channel,64,down?127:0);}
        if(e.type==EventType::Disconnect||e.type==EventType::Panic){for(auto i=externalNotes.begin();i!=externalNotes.end();){auto current=i++;if(e.type==EventType::Panic||std::get<0>(current->first)==e.source)release(current);}for(unsigned channel=0;channel<16;++channel){if(e.type==EventType::Panic)for(auto& source:externalPedals)source[channel]=false;else externalPedals[e.source][channel]=false;bool down=false;for(auto& source:externalPedals)down|=source[channel];send(0xb0|channel,64,down?127:0);}}
    }
    if(events.push(e))return true;++overload;
    if(e.type==EventType::Panic)emergencyPanic=true;
    else if(e.type!=EventType::On){sourceGeneration[e.source].fetch_add(1);emergencySources[e.source/64].fetch_or(uint64_t{1}<<(e.source%64));}
    return false;
}
void Engine::Impl::publish(unsigned i){auto& n=notes[i];auto& p=published[i];p.sequence.fetch_add(1,std::memory_order_relaxed);uint64_t bits=n.active?1:0;if(n.active){bits|=(n.held?2:0)|(n.sounding?4:0)|(pedals[unsigned(n.event.bus)][n.event.source][n.event.channel]?8:0);bits|=uint64_t(n.event.note)<<8|uint64_t(n.event.velocity)<<16|uint64_t(n.event.source)<<24|uint64_t(n.event.channel)<<40|uint64_t(n.event.bus)<<44;}p.instance.store(n.event.instance,std::memory_order_relaxed);p.packed.store(bits,std::memory_order_relaxed);p.sequence.fetch_add(1,std::memory_order_release);}
void Engine::Impl::off(unsigned i,int delay){auto& n=notes[i];if(n.sounding&&pianoReady)synths[unsigned(n.event.bus)]->protectedNoteOff(delay,n.event.note,n.token);if(n.event.bus==Bus::Live&&n.held)--liveHeld;n.active=false;n.held=false;publish(i);}
void Engine::Impl::disconnect(unsigned source,int delay){for(unsigned b=0;b<3;++b)for(unsigned c=0;c<16;++c){if(pedals[b][source][c]&&b==0)--livePedals;pedals[b][source][c]=false;}for(unsigned i=0;i<NoteCount;++i)if(notes[i].active&&notes[i].event.source==source)off(i,delay);}
void Engine::Impl::panic(uint64_t absolute){metronome.panic();if(inputHook)inputHook({EventType::Panic},absolute==UINT64_MAX?frame.load():absolute,inputContext);if(pianoReady.load(std::memory_order_acquire))for(auto& s:synths)s->allSoundOff();for(unsigned i=0;i<NoteCount;++i){notes[i].active=false;notes[i].held=false;publish(i);}for(auto& b:pedals)for(auto& s:b)s.fill(false);liveHeld=0;livePedals=0;scheduledSize=0;}
void Engine::Impl::dispatch(const InputEvent& e,int delay,uint64_t absolute){
    if(e.type==EventType::Panic){panic(absolute);return;}
    if(e.type!=EventType::Disconnect&&e.generation!=sourceGeneration[e.source].load())return;
    if(e.type!=EventType::StopBus&&e.busGeneration!=busGeneration[unsigned(e.bus)].load())return;
    if(e.type==EventType::Disconnect){disconnect(e.source,delay);return;}
    if(e.type==EventType::StopBus){for(unsigned i=0;i<NoteCount;++i)if(notes[i].active&&notes[i].event.bus==e.bus)off(i,delay);for(auto& p:pedals[unsigned(e.bus)])p.fill(false);return;}
    unsigned b=unsigned(e.bus);
    if(e.type==EventType::Pedal){bool down=e.velocity>=64;auto& state=pedals[b][e.source][e.channel];if(state!=down&&b==0){if(down)++livePedals;else --livePedals;}state=down;for(unsigned i=0;i<NoteCount;++i)if(notes[i].active&&notes[i].event.bus==e.bus&&notes[i].event.source==e.source&&notes[i].event.channel==e.channel){if(!down&&!notes[i].held)off(i,delay);else publish(i);}}
    else if(e.type==EventType::Off){for(unsigned i=0;i<NoteCount;++i){auto& n=notes[i];if(!n.active||n.event.source!=e.source||n.event.bus!=e.bus||n.event.channel!=e.channel||n.event.instance!=e.instance)continue;if(n.held&&b==0)--liveHeld;n.held=false;if(!pedals[b][e.source][e.channel])off(i,delay);else publish(i);}}
    else if(e.type==EventType::On){unsigned slot=NoteCount;for(unsigned i=0;i<NoteCount;++i){auto& n=notes[i];if(n.active&&n.event.source==e.source&&n.event.bus==e.bus&&n.event.channel==e.channel&&n.event.instance==e.instance){++overload;return;}if(!n.active&&slot==NoteCount)slot=i;}if(slot==NoteCount){++overload;return;}auto& n=notes[slot];n={true,true,false,e,++tokenCounter};if(b==0)++liveHeld;bool audible=b!=0||(monitoring.load()&&outputMode.load()==0);if(audible&&pianoReady){int velocity=std::clamp(int(std::lround(std::pow(e.velocity/127.f,velocityCurve.load())*127)),1,127);n.sounding=synths[b]->protectedNoteOn(delay,e.note,velocity,n.token);if(!n.sounding)++overload;}publish(slot);}
    if(inputHook)inputHook(e,absolute,inputContext);
}
void Engine::Impl::render(float* output,unsigned frames){
    while(frames){unsigned count=std::min(frames,BlockFrames);uint64_t begin=frame.load();InputEvent event;
        if(!renderPianoReadySeen&&pianoReady.load(std::memory_order_acquire)){renderPianoReadySeen=true;for(unsigned i=0;i<NoteCount;++i){auto& n=notes[i];if(n.active&&!n.sounding&&(n.event.bus!=Bus::Live||(monitoring&&outputMode==0))){int velocity=std::clamp(int(std::lround(std::pow(n.event.velocity/127.f,velocityCurve.load())*127)),1,127);n.sounding=synths[unsigned(n.event.bus)]->protectedNoteOn(0,n.event.note,velocity,n.token);if(!n.sounding)++overload;publish(i);}}}
        auto later=[](const Scheduled& a,const Scheduled& b){return a.event.frame!=b.event.frame?a.event.frame>b.event.frame:a.order>b.order;};
        unsigned retained=0;
        for(unsigned i=0;i<scheduledSize;++i){const auto& e=scheduled[i].event;if(e.generation==sourceGeneration[e.source].load()&&e.busGeneration==busGeneration[unsigned(e.bus)].load())scheduled[retained++]=scheduled[i];}
        if(retained!=scheduledSize){scheduledSize=retained;std::make_heap(scheduled.begin(),scheduled.begin()+scheduledSize,later);}
        while(events.pop(event)){if(event.type!=EventType::Disconnect&&event.generation!=sourceGeneration[event.source].load())continue;if(event.type!=EventType::StopBus&&event.busGeneration!=busGeneration[unsigned(event.bus)].load())continue;if(event.frame==0||event.frame<begin)event.frame=begin;if(scheduledSize>=ScheduledCount||(event.type==EventType::On&&scheduledSize>=ScheduledCount-1024)){++overload;if(event.type!=EventType::On){sourceGeneration[event.source].fetch_add(1);emergencySources[event.source/64].fetch_or(uint64_t{1}<<(event.source%64));}continue;}scheduled[scheduledSize++]={event,++eventOrder};std::push_heap(scheduled.begin(),scheduled.begin()+scheduledSize,later);}
        while(scheduledSize&&scheduled[0].event.frame<begin+count){std::pop_heap(scheduled.begin(),scheduled.begin()+scheduledSize,later);auto e=scheduled[--scheduledSize].event;dispatch(e,int(e.frame-begin),e.frame);}
        for(unsigned w=0;w<2;++w){auto bits=emergencySources[w].exchange(0);for(unsigned k=0;k<64;++k)if(bits&(uint64_t{1}<<k))disconnect(w*64+k,0);}if(emergencyPanic.exchange(false))panic();
        std::fill_n(output,count*2,0.f);std::array<float,BlockFrames> left{},right{};float* channels[]{left.data(),right.data()};
        if(pianoReady.load(std::memory_order_acquire))for(auto& s:synths){s->renderBlock(sfz::AudioSpan<float>(channels,2,0,count));for(unsigned i=0;i<count;++i){output[i*2]+=left[i]*0.25f;output[i*2+1]+=right[i]*0.25f;}}
        if(pianoReady){uint64_t refusals=0;for(auto& s:synths)refusals+=s->getProtectedRefusals();auto previous=voiceRefusals.exchange(refusals);overload.fetch_add(refusals-previous);}
        metronome.render(output,count,begin);if(renderHook)renderHook(output,count,begin,renderContext);float gain=volume.load();for(unsigned i=0;i<count*2;++i){float x=output[i]*gain;output[i]=std::isfinite(x)?x/(1.f+std::abs(x)):0.f;}
        frame.store(begin+count);frames-=count;output+=count*2;
    }
}
Engine::Engine(EngineOptions options):impl_(std::make_unique<Impl>(std::move(options))){}
Engine::~Engine(){stop();}
void Engine::render(float* output,unsigned frames){if(!output&&frames)throw std::invalid_argument("Null audio buffer");impl_->render(output,frames);}
bool Engine::submit(InputEvent event){return impl_->submit(event);}
uint64_t Engine::sampleFrame()const noexcept{return impl_->frame.load();}
unsigned Engine::sampleRate()const noexcept{return impl_->options.sampleRate;}
void Engine::setRenderHook(RenderHook hook,void* context){if(impl_->running)throw std::logic_error("Register render hook before start");impl_->renderHook=hook;impl_->renderContext=context;}
void Engine::setInputHook(InputHook hook,void* context){if(impl_->running)throw std::logic_error("Register input hook before start");impl_->inputHook=hook;impl_->inputContext=context;}
}

void sightreading::Engine::setExternalMessageHook(ExternalMessageHook hook,void* context){if(impl_->running)throw std::logic_error("Register external message hook before start");std::lock_guard lock(impl_->externalControl);impl_->externalHook=hook;impl_->externalContext=context;}

void sightreading::Engine::setBeatHook(Metronome::BeatHook hook,void* context){if(impl_->running)throw std::logic_error("Register beat hook before start");impl_->metronome.hook=hook;impl_->metronome.context=context;}
void sightreading::Engine::followMetronome(double tempo,unsigned numerator,unsigned denominator,unsigned beat,uint64_t frame){std::lock_guard lock(impl_->control);impl_->metronome.follow(tempo,numerator,denominator,beat,frame);}
