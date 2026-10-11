#include "EngineInternal.hpp"
#include <cmath>

namespace sightreading {
namespace {
Bus parseBus(const json& p){auto name=p.value("bus",std::string("live"));if(name=="live")return Bus::Live;if(name=="playback")return Bus::Playback;if(name=="example")return Bus::Example;throw std::invalid_argument("bus must be live, playback, or example");}
unsigned integer(const json& p,const char* key,unsigned low,unsigned high,unsigned fallback){if(!p.contains(key))return fallback;if(!p[key].is_number_integer())throw std::invalid_argument(std::string(key)+" must be an integer");auto value=p[key].get<int64_t>();if(value<low||value>high)throw std::invalid_argument(std::string(key)+" out of range");return unsigned(value);}
uint64_t instance(const json& p){if(!p.contains("instance")||!p["instance"].is_number_integer())throw std::invalid_argument("instance must be a positive integer");auto n=p["instance"].get<int64_t>();if(n<=0||n>0x0000ffffffffffffLL)throw std::invalid_argument("instance out of range");return n;}
void validateSettings(const json& proposed){
    for(auto key:{"volume","velocityCurve"})if(!proposed[key].is_number()||!std::isfinite(proposed[key].get<double>()))throw std::invalid_argument(std::string(key)+" must be finite");
    if(proposed["volume"].get<double>()<0||proposed["volume"].get<double>()>1)throw std::invalid_argument("volume out of range");
    if(proposed["velocityCurve"].get<double>()<0.25||proposed["velocityCurve"].get<double>()>4)throw std::invalid_argument("velocityCurve out of range");
    if(!proposed["monitoring"].is_boolean()||!proposed["exclusive"].is_boolean())throw std::invalid_argument("monitoring/exclusive must be boolean");
    if(!proposed["outputMode"].is_string()||(proposed["outputMode"]!="internal"&&proposed["outputMode"]!="external"&&proposed["outputMode"]!="silent"))throw std::invalid_argument("Invalid outputMode");
    if(!proposed["channels"].is_array())throw std::invalid_argument("channels must be an array");for(auto& c:proposed["channels"])if(!c.is_number_integer()||c.get<int>()<0||c.get<int>()>15)throw std::invalid_argument("channel out of range");
    for(auto key:{"outputDevice","externalDevice"})if(!proposed[key].is_number_integer()||proposed[key].get<int>()<-1)throw std::invalid_argument(std::string(key)+" out of range");
}
}
json Engine::command(const json& request){
    auto& s=*impl_;json id=request.is_object()?request.value("id",json()):json();auto reply=[&](bool ok,json result,json error){return json{{"version",1},{"id",id},{"ok",ok},{"result",result},{"error",error}};};
    try{
        if(!request.is_object()||!request.contains("id")||!request.contains("version")||request["version"]!=1||!request.contains("type")||!request["type"].is_string())return reply(false,nullptr,{{"code","invalid_request"},{"message","Expected version 1 request with id and type"}});
        auto type=request["type"].get<std::string>();json p=request.value("params",json::object());if(p.is_null())p=json::object();if(!p.is_object())throw std::invalid_argument("params must be an object");std::lock_guard lock(s.control);
        if(type=="status")return reply(true,snapshot(),nullptr);
        if(type=="panic"){s.submit({EventType::Panic});return reply(true,json::object(),nullptr);}
        if(type=="virtual.noteOn"||type=="virtual.noteOff"||type=="virtual.pedal"||type=="virtual.disconnect"){
            InputEvent event{};event.source=s.source(p.at("source").get<std::string>());event.channel=integer(p,"channel",0,15,0);event.bus=parseBus(p);
            if(type=="virtual.noteOn"){if(!s.pianoReady&&s.outputMode==0)return reply(false,nullptr,{{"code","piano_not_ready"},{"message",s.error.empty()?"Piano is preloading":s.error}});event.type=EventType::On;event.note=integer(p,"note",21,108,0);if(!event.note)throw std::invalid_argument("note is required");event.velocity=integer(p,"velocity",1,127,100);event.instance=instance(p);}
            else if(type=="virtual.noteOff"){event.type=EventType::Off;event.instance=instance(p);}
            else if(type=="virtual.pedal"){event.type=EventType::Pedal;auto down=p.at("down");if(!down.is_boolean())throw std::invalid_argument("down must be boolean");event.velocity=down.get<bool>()?127:0;}
            else event.type=EventType::Disconnect;
            if(!s.submit(event))return reply(false,nullptr,{{"code","overload"},{"message","Native event queue pressure; source release recovery scheduled"}});
            return reply(true,{{"queued",true},{"sourceId",event.source}},nullptr);
        }
        if(type=="bus.stop"){auto bus=parseBus(p);if(bus==Bus::Live)return reply(false,nullptr,{{"code","protected_bus"},{"message","Live notes require source-scoped release or explicit Panic"}});if(!s.submit({EventType::StopBus,bus}))return reply(false,nullptr,{{"code","overload"},{"message","Stop queue pressure"}});return reply(true,{{"queued",true}},nullptr);}
        if(type=="devices.enable"){auto name=p.at("id").get<std::string>();auto enabled=p.at("enabled");if(!enabled.is_boolean())throw std::invalid_argument("enabled must be boolean");bool found=false;for(const auto& d:s.devices)if(d["id"]==name)found=true;if(!found)throw std::invalid_argument("Unknown MIDI input");s.disabled[name]=!enabled.get<bool>();s.pollMidi();return reply(true,s.devices,nullptr);}
        if(type=="audio.applyPending")return reply(true,{{"applied",s.applyPending(p.value("applyNow",false))}},nullptr);
        if(type=="audio.settings"){
            json proposed=s.pending.is_null()?s.settings:s.pending;for(auto i=p.begin();i!=p.end();++i){if(!proposed.contains(i.key()))throw std::invalid_argument("Unknown audio setting: "+i.key());proposed[i.key()]=i.value();}validateSettings(proposed);
            bool disruptive=false;for(auto key:{"outputDevice","exclusive","outputMode","monitoring","externalDevice"})if(proposed[key]!=s.settings[key])disruptive=true;
            if(disruptive&&(s.liveHeld||s.livePedals)){s.pending=proposed;return reply(true,{{"deferred",true}},nullptr);}
            s.settings=proposed;s.pending=nullptr;s.applySettings();if(disruptive&&s.running)s.openAudio();return reply(true,{{"deferred",false}},nullptr);
        }
        return reply(false,nullptr,{{"code","unknown_command"},{"message","Unknown native command"}});
    }catch(const std::exception& e){return reply(false,nullptr,{{"code","invalid_params"},{"message",e.what()}});}
}
json Engine::snapshot()const{
    auto& s=*impl_;std::lock_guard lock(s.control);json held=json::array();
    for(const auto& p:s.published){auto before=p.sequence.load(std::memory_order_acquire);if(before&1)continue;auto bits=p.packed.load();auto instance=p.instance.load();if(before!=p.sequence.load(std::memory_order_acquire)||!(bits&1))continue;unsigned source=(bits>>24)&0xffff;Bus bus=static_cast<Bus>((bits>>44)&3);held.push_back({{"source",s.sourceNames[source].empty()?"native:"+std::to_string(source):s.sourceNames[source]},{"sourceId",source},{"channel",(bits>>40)&15},{"note",(bits>>8)&255},{"velocity",(bits>>16)&255},{"instance",instance},{"bus",busName(bus)},{"held",bool(bits&2)},{"sounding",bool(bits&4)},{"sustained",bool(bits&8)&&!(bits&2)}});}
    json messages=json::array();auto count=s.monitorCount.load();for(uint64_t i=count>64?count-64:0;i<count;++i){auto m=s.monitorMessages[i%64].load();messages.push_back({{"status",m&255},{"data1",(m>>8)&255},{"data2",(m>>16)&255},{"sourceId",(m>>24)&255},{"sampleFrame",m>>32}});}
    return {{"audioReady",s.audioReady.load()},{"pianoReady",s.pianoReady.load()},{"pianoLoading",s.pianoLoading.load()},{"error",s.error.empty()?json(nullptr):json(s.error)},{"devices",s.devices},{"midiOutputs",s.outputs},{"audioOutputs",s.audioOutputs},{"heldNotes",held},{"monitor",messages},{"overload",s.overload.load()},{"voiceRefusals",s.voiceRefusals.load()},{"dropouts",s.dropouts.load()},{"externalErrors",s.externalErrors.load()},{"pendingChange",s.pending},{"settings",s.settings},{"sampleFrame",s.frame.load()},{"sampleRate",s.options.sampleRate},{"output",{{"backend","WASAPI"},{"requestedSampleRate",s.options.sampleRate},{"actualSampleRate",s.actualRate},{"requestedFrames",s.options.blockFrames},{"actualFrames",s.actualFrames.load()},{"latencyMs",s.outputLatency*1000},{"name",s.outputName},{"exclusive",s.settings["exclusive"]}}},{"livePool",{{"voices",1024},{"attackCapacity",960},{"releaseReserve",64},{"stealing",false}}}};
}
}
