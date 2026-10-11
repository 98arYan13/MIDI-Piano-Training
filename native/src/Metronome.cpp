#include <sightreading/Metronome.hpp>
#include <cmath>
#include <stdexcept>
namespace sightreading {
using json=nlohmann::json;
Metronome::Metronome(unsigned r):rate(r){for(unsigned s=0;s<3;++s)for(unsigned i=0;i<2400;++i){double t=double(i)/rate;double f=s==0?2200:s==1?850:1400;sounds[s][i]=float((std::cos(t*f*6.283185307)+0.3*std::cos(t*(f*1.71)*6.283185307))*std::exp(-t*(s==1?180:300))*0.25);}}
bool Metronome::push(Change c){auto w=write.load();if(w-read.load(std::memory_order_acquire)>=queue.size()-1)return false;c.sequence=w+1;queue[w%queue.size()]=c;write.store(w+1,std::memory_order_release);return true;}
json Metronome::command(const std::string& type,const json& p){
 auto c=requested;
 if(p.contains("meter")){const auto& m=p["meter"];if(!m.is_object()||!m.contains("numerator")||!m.contains("denominator")||!m["numerator"].is_number_integer()||!m["denominator"].is_number_integer())throw std::invalid_argument("meter needs integer numerator and denominator");int n=m["numerator"],d=m["denominator"];if(n<1||n>12||(d!=2&&d!=4&&d!=8&&d!=16))throw std::invalid_argument("meter out of range");c.numerator=n;c.denominator=d;}
 if(type=="metronome.config"){
  for(auto i=p.begin();i!=p.end();++i){auto k=i.key();auto v=i.value();
   if(k=="tempo"||k=="swing"||k=="volume"){if(!v.is_number()||!std::isfinite(v.get<double>()))throw std::invalid_argument("metronome numeric value must be finite");double x=v.get<double>();if(k=="tempo"){if(x<20||x>300)throw std::invalid_argument("tempo out of range");c.tempo=x;}else if(k=="swing"){if(x<50||x>75)throw std::invalid_argument("swing out of range");c.swing=x;}else{if(x<0||x>1)throw std::invalid_argument("volume out of range");c.volume=float(x);}}
   else if(k=="beatUnit"){if(!v.is_number_integer()||v.get<int>()!=int(c.denominator))throw std::invalid_argument("beatUnit must match meter denominator");}
   else if(k=="meter"){if(!v.is_object()||!v.contains("numerator")||!v.contains("denominator")||!v["numerator"].is_number_integer()||!v["denominator"].is_number_integer())throw std::invalid_argument("meter needs integer numerator and denominator");int n=v["numerator"],d=v["denominator"];if(n<1||n>12||(d!=2&&d!=4&&d!=8&&d!=16))throw std::invalid_argument("meter out of range");c.numerator=n;c.denominator=d;}
   else if(k=="subdivision"){auto s=v.get<std::string>();if(s=="quarter")c.subdivision=1;else if(s=="eighth")c.subdivision=2;else if(s=="sixteenth")c.subdivision=4;else if(s=="triplet")c.subdivision=3;else throw std::invalid_argument("invalid subdivision");}
   else if(k=="accents"){if(!v.is_array()||v.size()!=c.numerator)throw std::invalid_argument("one accent required per beat");for(unsigned j=0;j<v.size();++j){auto s=v[j].get<std::string>();if(s=="strong")c.accents[j]=2;else if(s=="weak")c.accents[j]=1;else if(s=="mute")c.accents[j]=0;else throw std::invalid_argument("invalid accent");}}
   else if(k=="countIn"){if(!v.is_number_integer()||v.get<int>()<0||v.get<int>()>4)throw std::invalid_argument("countIn out of range");c.countIn=v.get<unsigned>();}
   else if(k=="sound"){auto s=v.get<std::string>();if(s=="click")c.sound=0;else if(s=="woodblock")c.sound=1;else if(s=="rim")c.sound=2;else throw std::invalid_argument("invalid sound");}
   else if(k=="mode"){auto s=v.get<std::string>();if(s!="independent"&&s!="follow")throw std::invalid_argument("invalid mode");c.follow=s=="follow";}
   else if(k=="mute"){if(!v.is_boolean())throw std::invalid_argument("mute must be boolean");c.mute=v.get<bool>();}else throw std::invalid_argument("unknown metronome setting");
  }
 }
 unsigned op=type=="metronome.start"?1:type=="metronome.stop"?2:0;
 if(!push({c,op,0,0,0,unsigned(p.contains("tempo"))|unsigned(p.contains("meter"))*2}))throw std::runtime_error("metronome command queue full");requested=c;return snapshot();
}
json Metronome::snapshot()const{json accents=json::array();for(unsigned i=0;i<requested.numerator;++i)accents.push_back(requested.accents[i]==2?"strong":requested.accents[i]==1?"weak":"mute");json settings={{"mode",requested.follow?"follow":"independent"},{"tempo",requested.tempo},{"beatUnit",requested.denominator},{"meter",{{"numerator",requested.numerator},{"denominator",requested.denominator}}},{"subdivision",requested.subdivision==1?"quarter":requested.subdivision==2?"eighth":requested.subdivision==3?"triplet":"sixteenth"},{"accents",accents},{"swing",requested.swing},{"countIn",requested.countIn},{"sound",requested.sound==0?"click":requested.sound==1?"woodblock":"rim"},{"volume",requested.volume},{"mute",requested.mute}};auto state=settings;state["running"]=publishedRunning.load();state["activeTempo"]=publishedTempo.load();state["activeMeter"]={{"numerator",publishedNumerator.load()},{"denominator",publishedDenominator.load()}};state["beat"]=publishedBeat.load();state["bar"]=publishedBar.load();state["settings"]=settings;return state;}
void Metronome::follow(double tempo,unsigned n,unsigned d,unsigned b,uint64_t f){if(!requested.follow)return;if(tempo<20||tempo>300||!std::isfinite(tempo)||n<1||n>12||(d!=2&&d!=4&&d!=8&&d!=16)||b>=n)throw std::invalid_argument("invalid follow transport");auto c=requested;c.tempo=tempo;c.numerator=n;c.denominator=d;if(followOutstanding.load()>=63)throw std::runtime_error("metronome follow schedule full");followOutstanding.fetch_add(1);if(!push({c,3,b,f})){followOutstanding.fetch_sub(1);throw std::runtime_error("metronome command queue full");}requested=c;}
void Metronome::panic(uint64_t boundary)noexcept{running=false;publishedRunning=false;for(auto& v:voices)v.active=false;uint64_t limit=boundary==UINT64_MAX?write.load(std::memory_order_acquire):boundary;if(limit>canceled)canceled=limit;unsigned kept=0;for(unsigned i=0;i<scheduledSize;++i){if(scheduled[i].sequence<=canceled)followOutstanding.fetch_sub(1);else scheduled[kept++]=scheduled[i];}scheduledSize=kept;}
void Metronome::render(float* out,unsigned frames,uint64_t start)noexcept{
 auto r=read.load();auto w=write.load(std::memory_order_acquire);while(r!=w){auto c=queue[r%queue.size()];++r;if(c.sequence<=canceled){if(c.operation==3)followOutstanding.fetch_sub(1);continue;}if(c.operation==3){unsigned at=scheduledSize;while(at&&scheduled[at-1].frame>c.frame){scheduled[at]=scheduled[at-1];--at;}scheduled[at]=c;++scheduledSize;continue;}target=c.config;pending=true;if(c.changedFields&1)tempoConfigSequence=c.sequence;if(c.changedFields&2)meterConfigSequence=c.sequence;if(c.operation==2){running=false;for(auto& v:voices)v.active=false;followOutstanding.fetch_sub(scheduledSize);scheduledSize=0;}else if(c.operation==1&&!running){active=target;pending=false;running=true;beat=part=0;bar=0;countBars=active.countIn;next=double(start);}}read.store(r,std::memory_order_release);
 for(unsigned i=0;i<frames;++i){uint64_t f=start+i;
  while(scheduledSize&&scheduled[0].frame<=f){auto c=scheduled[0];for(unsigned j=1;j<scheduledSize;++j)scheduled[j-1]=scheduled[j];--scheduledSize;followOutstanding.fetch_sub(1);if(running&&active.follow){if(c.sequence>tempoConfigSequence){active.tempo=c.config.tempo;target.tempo=c.config.tempo;}if(c.sequence>meterConfigSequence){active.numerator=c.config.numerator;active.denominator=c.config.denominator;target.numerator=c.config.numerator;target.denominator=c.config.denominator;}beat=c.beat%active.numerator;part=0;next=double(f);}}
  if(running&&f>=uint64_t(std::llround(next))){
   if(part==0){if(pending){active.tempo=target.tempo;active.volume=target.volume;active.mute=target.mute;active.sound=target.sound;active.follow=target.follow;if(beat==0){active=target;pending=false;}}beatStart=next;publishedTempo=active.tempo;publishedNumerator=active.numerator;publishedDenominator=active.denominator;publishedBeat=beat;publishedBar=bar;}
   BeatEvent event{f,bar,beat,part,bar<countBars};if(hook)hook(event,context);
   unsigned accent=active.accents[beat];if(accent&&!active.mute)for(auto& v:voices)if(!v.active){v={active.sound,0,active.volume*(part==0?(accent==2?1.f:0.55f):0.3f),true};break;}
   double duration=rate*60.0/active.tempo*4.0/active.denominator;
   if(++part==active.subdivision){part=0;next=beatStart+duration;if(++beat==active.numerator){beat=0;++bar;}}
   else {double fraction=double(part)/active.subdivision;if(active.subdivision==2)fraction=active.swing/100.;else if(active.subdivision==4)fraction=part==1?active.swing/200.:part==2?0.5:0.5+active.swing/200.;next=beatStart+duration*fraction;}
  }
  float sample=0;for(auto& v:voices)if(v.active){sample+=sounds[v.sound][v.position++]*v.gain;if(v.position==2400)v.active=false;}out[i*2]+=sample;out[i*2+1]+=sample;
 }publishedRunning=running;
}
}
