import assert from 'node:assert/strict';
import {spawn} from 'node:child_process';
import {mkdtemp,rm,readFile,writeFile,mkdir} from 'node:fs/promises';
import {tmpdir} from 'node:os';
import {join,resolve} from 'node:path';
import {chromium} from '../web/node_modules/playwright/index.mjs';
import {browserPath} from './browser-path.mjs';
import {checkPortableDocLinks} from './package-links.mjs';
import {checkFingeringPackage,checkFingeringPackageFailures} from './fingering-package.mjs';
const release=resolve(process.env.SR_RELEASE ?? 'build/release/SightReading'),port=18782,url=`http://127.0.0.1:${port}`;
const data=await mkdtemp(join(tmpdir(),'sr-release-'));
let child,browser,log='',request=0;
const pause=ms=>new Promise(r=>setTimeout(r,ms));
const state=async()=>await(await fetch(url+'/api/v1/status')).json();
const until=async fn=>{for(let i=0;i<200;i++){if(child?.exitCode!==null && child?.exitCode!==undefined)throw Error(log);try{if(await fn())return;}catch{}await pause(100);}throw Error('Release readiness timeout '+log);};
const command=async(type,params={})=>{const r=await(await fetch(url+'/api/v1/command',{method:'POST',headers:{Origin:url,'Content-Type':'application/json'},body:JSON.stringify({version:1,id:++request,type,params})})).json();assert.equal(r.ok,true,JSON.stringify(r));return r.result;};
const launch=()=>{child=spawn(join(release,'sightreading.exe'),['--port',String(port),'--data-dir',data,'--no-tray'],{cwd:release,env:{...process.env,PATH:join(process.env.SystemRoot,'System32')},stdio:'pipe'});child.on('error',e=>log+=e);child.stdout.on('data',s=>log+=s);child.stderr.on('data',s=>log+=s);};
const stop=async()=>{if(child?.exitCode===null){const done=new Promise(r=>child.once('exit',r));await fetch(url+'/api/v1/shutdown',{method:'POST',headers:{Origin:url}});await Promise.race([done,pause(15000).then(()=>{throw Error('Graceful shutdown timeout');})]);assert.equal(child.exitCode,0,log);}};
function vlq(n){let a=[n&127];while(n>>=7)a.unshift((n&127)|128);return a;}
function denseMidi(){let events=[];for(let i=0;i<4800;i++){for(let p=0;p<4;p++)events.push(...vlq(p?0:i?90:0),0x90,48+(i%12)+p*12,80);for(let p=0;p<4;p++)events.push(...vlq(p?0:30),0x80,48+(i%12)+p*12,0);}events.push(90,0xff,0x2f,0);const body=Buffer.from(events),head=Buffer.from('4d546864000000060000000101e04d54726b00000000','hex');head.writeUInt32BE(body.length,18);return Buffer.concat([head,body]);}
function fingeringMidi(){
 const tracks=[60,48].map(base=>{
  const bytes=[];for(let i=0;i<16;i++)bytes.push(0,0x90,base+i%5,80,...vlq(480),0x80,base+i%5,0);
  bytes.push(0,255,47,0);const h=Buffer.alloc(8);h.write('MTrk');h.writeUInt32BE(bytes.length,4);return Buffer.concat([h,Buffer.from(bytes)]);
 });return Buffer.concat([Buffer.from('4d546864000000060001000201e0','hex'),...tracks]);
}
const report={runtimePath:release,cleanPath:join(process.env.SystemRoot,'System32'),hardwareMidi:false,audibleCapture:false};
async function restartProof(fingerSong,fingerEvents,song){
 await browser.close();browser=null;await stop();launch();await until(async()=> (await state()).pianoReady);
 const retained=await(await fetch(url+'/api/v1/songs')).json();assert(retained.some(s=>s.id===song.id&&s.title==='Retained imported library'));report.libraryRetained=true;
 const persisted=await(await fetch(url+`/api/v1/songs/${fingerSong.id}/events?fromTick=0&toTick=100000`)).json();assert.deepEqual(persisted.notes,fingerEvents.notes);report.fingering.restartRetained=true;await stop();
 await mkdir('build',{recursive:true});await writeFile('build/release-proof.json',JSON.stringify(report,null,2));console.log(JSON.stringify(report,null,2));
}
async function measureFrames(page) {
 const frames=await page.evaluate(()=>new Promise(resolve=>{const d=[],start=performance.now();let last=start;function frame(t){d.push(t-last);last=t;if(t-start>=5000)resolve(d.slice(1));else requestAnimationFrame(frame);}requestAnimationFrame(frame);}));
 const sorted=[...frames].sort((a,b)=>a-b);
 return {count:frames.length,durationMs:frames.reduce((a,b)=>a+b,0),medianMs:sorted[Math.floor(sorted.length*.5)],p95Ms:sorted[Math.floor(sorted.length*.95)],maxMs:Math.max(...frames),fps:1000*frames.length/frames.reduce((a,b)=>a+b,0)};
}
try{
 report.fingeringPackage=await checkFingeringPackage(release);
 report.fingeringPackageFaults=await checkFingeringPackageFailures(release);
 report.portableDocumentCount=await checkPortableDocLinks(release);
 const start=performance.now();launch();await until(async()=>{const s=await state();return s.audioReady&&s.pianoReady;});report.readyMs=performance.now()-start;report.output=(await state()).output;
 for(const route of ['/','/staff','/ear','/flash','/play','/guide'])assert.equal((await fetch(url+route)).status,200);
 const fingerForm=new FormData();fingerForm.append('file',new Blob([fingeringMidi()]),'Portable both hands.mid');
 const fingerResponse=await fetch(url+'/api/v1/songs/import',{method:'POST',headers:{Origin:url},body:fingerForm});assert.equal(fingerResponse.status,201);
 const fingerSong=await fingerResponse.json();await command('transport.load',{id:fingerSong.id});await command('transport.seek',{tick:480});await command('transport.pause');
 const transportBefore=(await state()).transport;
 const generateBegin=performance.now();const generate=await fetch(url+`/api/v1/songs/${fingerSong.id}/fingering/generate`,{method:'POST',headers:{Origin:url,'Content-Type':'application/json'},body:'{}'});assert.equal(generate.status,202);
 let fingerStatus;await until(async()=>{fingerStatus=await(await fetch(url+`/api/v1/songs/${fingerSong.id}/fingering`)).json();return fingerStatus.job.state!=='running';});
 assert.equal(fingerStatus.job.state,'completed',JSON.stringify(fingerStatus));
 const fingerEvents=await(await fetch(url+`/api/v1/songs/${fingerSong.id}/events?fromTick=0&toTick=100000`)).json();
 for(const hand of ['left','right'])assert(fingerEvents.notes.some(n=>n.fingering?.hand===hand&&n.fingering.finger>=1));
 const musicalTransport=({engineTimeNs,sampleFrame,...transport})=>transport;
 assert.deepEqual(musicalTransport((await state()).transport),musicalTransport(transportBefore));
 report.fingering={generationMs:performance.now()-generateBegin,summary:fingerStatus.summary,transportBefore:musicalTransport(transportBefore),transportAfter:musicalTransport((await state()).transport)};
 const form=new FormData();form.append('file',new Blob([denseMidi()]),'Dense ten-minute.mid');const response=await fetch(url+'/api/v1/songs/import',{method:'POST',headers:{Origin:url},body:form});assert.equal(response.status,201);const song=await response.json();assert.equal(song.notes.length,19200);assert(Math.abs(song.durationSeconds-600)<.1);
 const windows=[];for(let i=0;i<101;i++){const begin=performance.now();const r=await(await fetch(url+`/api/v1/songs/${song.id}/events?fromTick=470000&toTick=474000`)).json();assert(r.notes.length>100);assert(r.inspected<1000);windows.push(performance.now()-begin);}report.firstWindowMs=windows.shift();report.cachedWindowMedianMs=[...windows].sort((a,b)=>a-b)[50];report.cachedWindowMaxMs=Math.max(...windows);
 await fetch(url+`/api/v1/songs/${song.id}`,{method:'PATCH',headers:{Origin:url,'Content-Type':'application/json'},body:JSON.stringify({title:'Retained imported library',tracks:[{id:0,staff:'bass'}]})});const changed=await(await fetch(url+`/api/v1/songs/${song.id}/events?fromTick=470000&toTick=474000`)).json();assert(changed.notes.every(n=>n.staff==='bass'));
 browser=await chromium.launch({executablePath:browserPath(),headless:true});const context=await browser.newContext({viewport:{width:1265,height:713}});const page=await context.newPage();const remote=[],errors=[];page.on('pageerror',e=>errors.push(e.message));await context.route('**/*',route=>{if(!route.request().url().startsWith(url)){remote.push(route.request().url());return route.abort();}return route.continue();});
 report.hostBaselineFrames=await measureFrames(page);
 const cold=performance.now();await page.goto(url);await page.getByRole('img',{name:/Staff practice notation/}).waitFor();report.coldUiMs=performance.now()-cold;assert(report.coldUiMs<=2000,`Cold UI ${report.coldUiMs} ms`);
 for(const route of ['ear','flash','play','guide','staff']){await page.goto(url+'/#'+route);await page.getByRole('link',{name:route==='ear'?/Ear Training/:route==='flash'?/Flash Cards/:route==='play'?/Play Along/:route==='guide'?/Guide/:/Staff/}).waitFor();}
 await page.goto(url+'/#play');await page.getByRole('button',{name:/^Portable both hands/}).click();await page.getByLabel('Fingering tips').selectOption('full');await until(async()=>await page.locator('.vf-score-finger').count()>0);
 for(const hand of ['left','right'])await until(async()=>await page.locator(".vf-score-finger[data-fingering-hand='"+hand+"']" ).count()>0);await pause(500);report.fingering.visibleLabels=await page.locator('.vf-score-finger').count();await page.locator('.engraved-score').screenshot({path:'web/test-results/release-fingering.png'});
 if(process.env.SR_RELEASE_FINGERING_ONLY==='1'){report.scope='fingering-package';report.remoteRequests=remote.length;report.browserErrors=errors;assert.deepEqual(remote,[]);assert.deepEqual(errors,[]);await restartProof(fingerSong,fingerEvents,song);}else{
 await page.goto(url+'/#staff');await command('practice.configure',{staff:'grand'});await command('practice.start');await pause(300);await page.getByRole('img',{name:/joined treble and bass/}).waitFor();report.staffKeyboardBottom=await page.locator('.piano-keys').evaluate(e=>e.getBoundingClientRect().bottom);await page.screenshot({path:'web/test-results/release-grand.png'}); console.log(JSON.stringify(await page.locator('main,.workspace-heading,.workspace-stage,.staff-practice,.piano-panel,.piano-keys').evaluateAll(es=>es.map(e=>({class:e.className,rect:e.getBoundingClientRect().toJSON(),padding:getComputedStyle(e).padding}))))); assert(report.staffKeyboardBottom<=713,'Grand + keyboard bottom '+report.staffKeyboardBottom);
 await page.screenshot({path:'web/test-results/release-grand.png'});
 await page.goto(url+'/#play');await page.getByRole('button',{name:/^Retained imported library/}).click();await command('transport.configure',{practiceEnabled:false,hearPractice:true});await command('transport.seek',{tick:470000});await command('transport.play');await page.getByRole('group',{name:/Song practice notation/}).waitFor();
 report.notationFrames=await measureFrames(page);assert(report.notationFrames.maxMs<250,'Sustained notation stall');
 await command('transport.configure',{speed:.25,spacing:40});await command('transport.seek',{tick:470000});await pause(750);
 report.cachedNotationFrames=await measureFrames(page);
 assert(report.cachedNotationFrames.fps>=report.hostBaselineFrames.fps*.95,'Cached notation substantially below host baseline');
 report.playKeyboardBottom=await page.locator('.piano-keys').evaluate(e=>e.getBoundingClientRect().bottom);await page.screenshot({path:'web/test-results/release-play.png'});assert(report.playKeyboardBottom<=713,'Play + keyboard below viewport');assert.deepEqual(remote,[]);assert.deepEqual(errors,[]);
 await browser.close();browser=null;await stop();launch();await until(async()=> (await state()).pianoReady);const retained=await(await fetch(url+'/api/v1/songs')).json();assert(retained.some(s=>s.id===song.id&&s.title==='Retained imported library'));report.libraryRetained=true;
 const persistedFingerEvents=await(await fetch(url+`/api/v1/songs/${fingerSong.id}/events?fromTick=0&toTick=100000`)).json();assert.deepEqual(persistedFingerEvents.notes,fingerEvents.notes);report.fingering.restartRetained=true;await stop();
 await mkdir('build',{recursive:true});await writeFile('build/release-proof.json',JSON.stringify(report,null,2));console.log(JSON.stringify(report,null,2));
}
}catch(error){report.failure=error.message;await writeFile('build/release-proof.json',JSON.stringify(report,null,2));throw error;}finally{await browser?.close();if(child?.exitCode===null){try{await stop();}catch{child.kill();}}await rm(data,{recursive:true,force:true});}
