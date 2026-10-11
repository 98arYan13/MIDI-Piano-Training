import assert from 'node:assert/strict';
import {writeFile} from 'node:fs/promises';
const url=process.env.SR_URL ?? 'http://127.0.0.1:18780';
const minutes=Number(process.env.SR_SOAK_MINUTES ?? 30);
const wait=ms=>new Promise(r=>setTimeout(r,ms));
let sequence=0;
async function command(type,params={}) {
 const reply=await (await fetch(url+'/api/v1/command',{method:'POST',headers:{Origin:url,'Content-Type':'application/json'},body:JSON.stringify({version:1,id:++sequence,type,params})})).json();
 assert.equal(reply.ok,true,JSON.stringify(reply)); return reply.result;
}
const state=async()=>await(await fetch(url+'/api/v1/status')).json();
for(let i=0;i<300;i++){try{if((await state()).pianoReady)break;}catch{} await wait(100);}
const initial=await state(); assert.equal(initial.pianoReady,true); assert.equal(initial.audioReady,true);
await command('metronome.config',{countIn:0,tempo:137,subdivision:'sixteenth'});
await command('metronome.start');
const songs=await(await fetch(url+'/api/v1/songs')).json();
const list=Array.isArray(songs)?songs:songs.songs;
await command('transport.load',{id:list[0].id});
await command('transport.configure',{loop:true,a:0,b:1920,hearPractice:true,practiceEnabled:false,accompaniment:'Triad'});
await command('transport.play');
await command('virtual.noteOn',{source:'soak-retained',channel:0,note:60,velocity:90,instance:1});
const started=new Date(), readings=[];
console.log(JSON.stringify({started:started.toISOString(),minutes,initial:initial.output}));
let tick=0;
while(Date.now()-started.getTime()<minutes*60000){
 const instance=++tick+1,note=48+tick%36;
 await command('virtual.noteOn',{source:'soak',channel:0,note,velocity:80+(tick%40),instance});
 if(tick%15===1)await command('ear.play');
 await wait(240);
 await command('virtual.noteOff',{source:'soak',channel:0,note,instance});
 if(tick%100===0){const s=await state(); assert(s.heldNotes.some(n=>n.source==='soak-retained'&&n.instance===1&&n.held));assert(s.metronome.running);readings.push({elapsedMs:Date.now()-started.getTime(),sampleFrame:s.sampleFrame,dropouts:s.dropouts,overload:s.overload,voiceRefusals:s.voiceRefusals});console.log(JSON.stringify(readings.at(-1)));}
}
const final=await state();
await command('virtual.disconnect',{source:'soak'});await command('virtual.disconnect',{source:'soak-retained'});await command('ear.stop');await command('transport.stop');await command('metronome.stop');
const report={started:started.toISOString(),ended:new Date().toISOString(),elapsedMs:Date.now()-started.getTime(),iterations:tick,initial,final,readings,audibleCapture:false,physicalMidi:false};
await writeFile(process.env.SR_SOAK_REPORT ?? 'build/soak.json',JSON.stringify(report,null,2));
console.log(JSON.stringify({elapsedMs:report.elapsedMs,dropouts:final.dropouts-initial.dropouts,overload:final.overload-initial.overload,voiceRefusals:final.voiceRefusals-initial.voiceRefusals,retainedLive:true}));
