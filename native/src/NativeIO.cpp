#include "EngineInternal.hpp"
#include <chrono>
#include <windows.h>
#include <mmsystem.h>

namespace sightreading {
int Engine::Impl::callback(const void*,void* output,unsigned long frames,const PaStreamCallbackTimeInfo*,PaStreamCallbackFlags flags,void* context){auto& self=*static_cast<Impl*>(context);self.actualFrames=frames;if(flags&(paOutputUnderflow|paOutputOverflow))++self.dropouts;self.render(static_cast<float*>(output),unsigned(frames));return paContinue;}
void Engine::Impl::closeAudio(){audioReady=false;if(stream){Pa_StopStream(stream);Pa_CloseStream(stream);stream=nullptr;}if(pianoReady.load(std::memory_order_acquire))for(auto& s:synths)s->enableFreeWheeling();}
void Engine::Impl::openAudio(){
    closeAudio();
    if(!paInitialized){auto code=Pa_Initialize();if(code!=paNoError){error=Pa_GetErrorText(code);return;}paInitialized=true;}
    audioOutputs=json::array();for(int i=0;i<Pa_GetDeviceCount();++i){const auto* d=Pa_GetDeviceInfo(i);const auto* h=Pa_GetHostApiInfo(d->hostApi);if(d->maxOutputChannels>=2&&h->type==paWASAPI)audioOutputs.push_back({{"id",i},{"name",d->name},{"defaultSampleRate",d->defaultSampleRate}});}
    int device=settings["outputDevice"].get<int>();if(device<0){int host=Pa_HostApiTypeIdToHostApiIndex(paWASAPI);if(host>=0)device=Pa_GetHostApiInfo(host)->defaultOutputDevice;}
    if(device<0||device>=Pa_GetDeviceCount()){error="No WASAPI stereo output available";return;}
    const auto* d=Pa_GetDeviceInfo(device);const auto* h=Pa_GetHostApiInfo(d->hostApi);if(h->type!=paWASAPI||d->maxOutputChannels<2){error="Selected output must be a WASAPI stereo device";return;}
    PaWasapiStreamInfo info{};info.size=sizeof(info);info.hostApiType=paWASAPI;info.version=1;info.flags=settings["exclusive"].get<bool>()?paWinWasapiExclusive:paWinWasapiAutoConvert;
    PaStreamParameters output{};output.device=device;output.channelCount=2;output.sampleFormat=paFloat32;output.suggestedLatency=d->defaultLowOutputLatency;output.hostApiSpecificStreamInfo=&info;
    auto code=Pa_IsFormatSupported(nullptr,&output,options.sampleRate);if(code!=paFormatIsSupported){error="Requested sample format unsupported: "+std::string(Pa_GetErrorText(code));return;}
    code=Pa_OpenStream(&stream,nullptr,&output,options.sampleRate,options.blockFrames,paNoFlag,callback,this);if(code!=paNoError){error=Pa_GetErrorText(code);stream=nullptr;return;}
    if(pianoReady.load(std::memory_order_acquire))for(auto& s:synths)s->disableFreeWheeling();auto negotiated=Pa_GetStreamInfo(stream);actualRate=unsigned(negotiated->sampleRate);outputLatency=negotiated->outputLatency;outputName=d->name;
    code=Pa_StartStream(stream);if(code!=paNoError){error=Pa_GetErrorText(code);closeAudio();return;}error.clear();audioReady=true;
}
void Engine::Impl::midiCallback(double,std::vector<unsigned char>* msg,void* context){
    auto& port=*static_cast<InputPort*>(context);auto& self=*port.engine;if(msg->empty())return;
    unsigned status=(*msg)[0],channel=status&15,type=status&0xf0,note=msg->size()>1?(*msg)[1]:0,velocity=msg->size()>2?(*msg)[2]:0;
    auto index=self.monitorCount.fetch_add(1);self.monitorMessages[index%64].store(uint64_t(status)|(uint64_t(note)<<8)|(uint64_t(velocity)<<16)|(uint64_t(port.source)<<24)|(self.frame.load()<<32));
    if(msg->size()<3||note>127||velocity>127)return;InputEvent event{};event.source=port.source;event.channel=channel;event.note=note;event.velocity=velocity;
    if(type==0x90&&velocity){event.type=EventType::On;if(port.instances[channel][note]){auto old=event;old.type=EventType::Off;old.instance=port.instances[channel][note];self.submit(old);}event.instance=port.instances[channel][note]=++port.counter;}
    else if(type==0x80||(type==0x90&&!velocity)){event.type=EventType::Off;event.instance=port.instances[channel][note];port.instances[channel][note]=0;}
    else if(type==0xb0&&note==64)event.type=EventType::Pedal;else return;
    self.submit(event);
}
void Engine::Impl::pollMidi(){
    try{
        // Zero inputs is normal; avoid RtMidi's constructor warning on every poll.
        if(midiInGetNumDevs()==0){for(auto& [_,port]:ports){submit({EventType::Disconnect,Bus::Live,static_cast<uint16_t>(port->source)});port->input->cancelCallback();port->input->closePort();}ports.clear();devices=json::array();}
        else {
        RtMidiIn probe(RtMidi::WINDOWS_MM,"Sight Reading discovery");std::map<std::string,unsigned> available,duplicates;
        for(unsigned i=0;i<probe.getPortCount();++i){auto name=probe.getPortName(i);available[name+"#"+std::to_string(duplicates[name]++)]=i;}
        for(auto i=ports.begin();i!=ports.end();){if(!available.contains(i->first)||disabled[i->first]){submit({EventType::Disconnect,Bus::Live,static_cast<uint16_t>(i->second->source)});i->second->input->cancelCallback();i->second->input->closePort();i=ports.erase(i);}else ++i;}
        devices=json::array();
        for(const auto& [identity,index]:available){auto name=probe.getPortName(index);bool enabled=!disabled[identity];if(enabled&&!ports.contains(identity)){auto port=std::make_unique<InputPort>();port->engine=this;port->port=index;port->identity=identity;port->name=name;port->source=source("midi:"+identity);port->input=std::make_unique<RtMidiIn>(RtMidi::WINDOWS_MM,"Sight Reading input");port->input->ignoreTypes(true,true,true);port->input->setCallback(midiCallback,port.get());port->input->openPort(index);ports[identity]=std::move(port);}devices.push_back({{"id",identity},{"name",name},{"enabled",enabled},{"connected",ports.contains(identity)}});}
        }
        RtMidiOut outputProbe(RtMidi::WINDOWS_MM,"Sight Reading output discovery");outputs=json::array();for(unsigned i=0;i<outputProbe.getPortCount();++i)outputs.push_back({{"id",i},{"name",outputProbe.getPortName(i)}});
    }catch(const RtMidiError& e){error="MIDI: "+e.getMessage();}
}
void Engine::Impl::applySettings(){
    volume=settings["volume"].get<float>();monitoring=settings["monitoring"].get<bool>();velocityCurve=settings["velocityCurve"].get<float>();auto mode=settings["outputMode"].get<std::string>();outputMode=mode=="internal"?0:mode=="external"?1:2;
    uint16_t mask=0;for(const auto& c:settings["channels"])mask|=1u<<c.get<unsigned>();channelMask=mask;
    std::lock_guard outputLock(externalControl);int external=settings["externalDevice"].get<int>();if(external>=0&&(!midiOut||external!=openedExternal)){try{midiOut=std::make_unique<RtMidiOut>(RtMidi::WINDOWS_MM,"Sight Reading external");if(unsigned(external)>=midiOut->getPortCount())throw std::runtime_error("External MIDI output unavailable");midiOut->openPort(external);openedExternal=external;++externalPortOpens;}catch(const std::exception& e){midiOut.reset();openedExternal=-1;error=e.what();}}else if(external<0){midiOut.reset();openedExternal=-1;}
}
bool Engine::Impl::applyPending(bool now){
 if(pending.is_null())return true;
 if((liveHeld||livePedals)&&!now)return false;
 bool rebuild=pending["outputDevice"]!=settings["outputDevice"]||pending["exclusive"]!=settings["exclusive"];
 if(now){liveReconfiguring.store(true,std::memory_order_release);submit({EventType::ResetLive,Bus::Live});}
 settings=pending;pending=nullptr;applySettings();
 if(rebuild&&running)openAudio();
 if(now)liveReconfiguring.store(false,std::memory_order_release);
 return true;
}
void Engine::start(){
    auto& s=*impl_;std::lock_guard lock(s.control);if(s.running.exchange(true))return;s.pollMidi();
    s.openAudio();if(!s.pianoReady){s.pianoLoading=true;s.loader=std::thread([&s]{s.loadPiano();std::lock_guard guard(s.control);if(s.running&&!s.audioReady)s.openAudio();});}
    s.worker=std::thread([&s]{while(s.running){std::this_thread::sleep_for(std::chrono::milliseconds(250));std::lock_guard guard(s.control);if(!s.running)break;s.pollMidi();s.applyPending(false);}});
}
void Engine::stop(){auto& s=*impl_;s.running=false;if(s.loader.joinable())s.loader.join();if(s.worker.joinable())s.worker.join();std::lock_guard lock(s.control);for(auto& [_,p]:s.ports){p->input->cancelCallback();p->input->closePort();}s.ports.clear();s.submit({EventType::Panic});s.closeAudio();s.panic();if(s.paInitialized){Pa_Terminate();s.paInitialized=false;}std::lock_guard outputLock(s.externalControl);s.midiOut.reset();}
}
