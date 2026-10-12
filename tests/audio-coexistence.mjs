// Optional Windows hardware check. A separate process represents the movie app.
// Emits a quiet 997 Hz tone; loopback retains only its amplitude, no recording.
import { spawn } from "node:child_process";
import { once } from "node:events";
import { mkdtemp, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import assert from "node:assert/strict";
import { DatabaseSync } from "node:sqlite";
const probe="build/native/native/audio_coexistence_probe.exe";
const data=await mkdtemp(join(tmpdir(),"sightreading-coexistence-"));
const base="http://127.0.0.1:18769";
let tone,service;
async function measure(){
 const child=spawn(probe,[],{stdio:["ignore","pipe","pipe"]});let out="",err="";
 child.stdout.on("data",d=>out+=d);child.stderr.on("data",d=>err+=d);
 const [code]=await once(child,"exit");assert.equal(code,0,err);return JSON.parse(out).toneAmplitude;
}
try{
 // Reproduce an older installation that persisted exclusive device access.
 const legacy=new DatabaseSync(join(data,"practice.sqlite"));
 legacy.exec("CREATE TABLE settings(key TEXT PRIMARY KEY,value TEXT NOT NULL)");
 legacy.prepare("INSERT INTO settings(key,value) VALUES(?,?)").run("audio",JSON.stringify({exclusive:true,volume:1}));
 legacy.close();
 tone=spawn(probe,["--tone"],{stdio:["ignore","pipe","inherit"]});
 await Promise.race([once(tone.stdout,"data"),once(tone,"exit").then(([code])=>{throw Error(`Tone process exited (${code}); check whether another app holds exclusive audio access.`);})]);
 const before=await measure();assert.ok(before>0.00001,"Background tone must be audible before launch");
 service=spawn("build/native/native/sightreading.exe",["--port","18769","--data-dir",data,"--no-tray"],{stdio:"ignore"});
 let state;
 for(let i=0;i<150;i++){
  try{state=await(await fetch(base+"/api/v1/status")).json();if(state.audioReady&&state.pianoReady)break;}catch{}
  await new Promise(r=>setTimeout(r,100));
 }
 assert.ok(state?.audioReady&&state?.pianoReady,"Native engine must be ready");
 assert.equal(state.output.exclusive,false);
 assert.equal(state.settings.exclusive,false);
 assert.equal(state.settings.volume,1);
 const during=await measure();
 const reply=await(await fetch(base+"/api/v1/command",{method:"POST",headers:{Origin:base,"Content-Type":"application/json"},body:JSON.stringify({version:1,id:1,type:"virtual.noteOn",params:{source:"coexistence",note:60,velocity:100,instance:1}})})).json();
 assert.equal(reply.ok,true);
 const withPiano=await measure();
 await fetch(base+"/api/v1/shutdown",{method:"POST",headers:{Origin:base}});await once(service,"exit");service=null;
 const after=await measure();
 console.log(JSON.stringify({before,during,withPiano,after,duringRatio:during/before,pianoRatio:withPiano/before},null,2));
 assert.ok(during/before>0.9&&during/before<1.1,"Launching piano must not suppress the other process");
 assert.ok(withPiano/before>0.9&&withPiano/before<1.1,"Playing piano must not suppress the other process");
 assert.ok(after/before>0.9&&after/before<1.1,"Background sound must retain its original level");
}finally{
 if(service&&service.exitCode===null){service.kill();await once(service,"exit");}
 if(tone&&tone.exitCode===null){tone.kill();await once(tone,"exit");}
 await rm(data,{recursive:true,force:true});
}
