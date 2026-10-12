import assert from 'node:assert/strict';
import {spawn} from 'node:child_process';
import {mkdtemp,rm,writeFile} from 'node:fs/promises';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {chromium} from '../web/node_modules/playwright/index.mjs';
import {browserPath} from './browser-path.mjs';
const data=await mkdtemp(join(tmpdir(),'sr-final-notation-')),url='http://127.0.0.1:18785';
let child,browser,id=0,log='';const pause=ms=>new Promise(r=>setTimeout(r,ms));
const state=async()=>await(await fetch(url+'/api/v1/status')).json();
const command=async(type,params={})=>{const r=await(await fetch(url+'/api/v1/command',{method:'POST',headers:{Origin:url,'Content-Type':'application/json'},body:JSON.stringify({version:1,id:++id,type,params})})).json();assert.equal(r.ok,true,JSON.stringify(r));return r.result;};
const until=async fn=>{for(let i=0;i<150;i++){try{if(await fn())return;}catch{}await pause(100);}throw Error('Notation timeout '+log);};
const failures=[],proof={flash:[],seek:[]};
const check=(name,fn)=>{try{fn();}catch(e){failures.push(name+': '+e.message);}};
try{
 child=spawn('build/native/native/sightreading.exe',['--port','18785','--data-dir',data,'--no-tray'],{stdio:'pipe'});child.stdout.on('data',s=>log+=s);child.stderr.on('data',s=>log+=s);
 await until(async()=> (await state()).pianoReady);
 browser=await chromium.launch({executablePath:browserPath(),headless:true});const page=await browser.newPage({viewport:{width:1265,height:713}});
 await page.goto(url+'/#flash');await page.getByRole('img',{name:'Chord identification notation'}).waitFor({timeout:500}).catch(()=>{});
 for(const mode of ['treble','bass','grand']){
  await command('flash.configure',{mode:'chord',keys:['C'],inversions:[2],staff:mode});
  let result,seed;for(seed=1;seed<100;seed++){result=await command('flash.next',{seed});if(result.target.notes.some(n=>n.midiNote<60)&&result.target.notes.some(n=>n.midiNote>=60))break;}
  assert(seed<100);await until(async()=> (await page.locator('.scrolling-staff.'+mode).count())>0);await pause(250);
  const geometry=await page.locator('.staff-segment .vf-notehead').evaluateAll(es=>es.map(e=>({x:e.getBBox().x,y:e.getBBox().y,width:e.getBBox().width,height:e.getBBox().height})));
  proof.flash.push({mode,seed,notes:result.target.notes,geometry});
  check(mode+' complete triad',()=>assert.equal(geometry.length,3));check(mode+' glyph positions',()=>assert.equal(new Set(geometry.map(g=>g.y)).size,3));
  const expectedYs=result.target.notes.map(n=>{const clef=mode==='grand'?(n.midiNote<60?'bass':'treble'):mode;const staffY=mode==='grand'?(clef==='bass'?107:15):35;const diatonic=n.spelling.octave*7+'cdefgab'.indexOf(n.spelling.letter);return staffY+(clef==='bass'?90:150)-diatonic*5;}).sort((a,b)=>a-b);
  check(mode+' written glyph geometry',()=>assert.deepEqual(geometry.map(g=>g.y).sort((a,b)=>a-b),expectedYs));
  await page.evaluate(()=>window.cachedSvg=document.querySelector('.staff-segment svg'));await pause(400);
  assert(await page.evaluate(()=>window.cachedSvg===document.querySelector('.staff-segment svg')),'unchanged snapshots retain SVG');
  await command('flash.next',{seed:seed+1});await pause(250);assert(await page.evaluate(()=>window.cachedSvg!==document.querySelector('.staff-segment svg')),'changed Flash target invalidates SVG');
 }
 await page.goto(url+'/#play');const songs=await(await fetch(url+'/api/v1/songs')).json();assert(songs.length>=2);
 await command('transport.load',{id:songs[0].id});await command('transport.configure',{practiceEnabled:false,spacing:110});
 await until(async()=> (await page.locator('.song-workspace .staff-track').count())>0);
 const verifyPosition=async name=>{await pause(350);const s=(await state()).transport;const view=await page.locator('.song-workspace .scrolling-staff').evaluate(e=>({width:e.clientWidth,transform:e.querySelector('.staff-track').style.transform,segments:[...e.querySelectorAll('.staff-segment')].map(s=>Number(s.style.left.replace('px','')))}));const actual=Number(view.transform.match(/translate3d\(([-\d.]+)px/)[1]),expected=(view.width/2)-s.songSeconds*s.config.spacing;proof.seek.push({name,native:s,view,actual,expected});check(name,()=>assert(Math.abs(actual-expected)<1,`actual ${actual}, expected ${expected}`));const first=Math.max(0,Math.floor((s.songSeconds*s.config.spacing-(view.width/2))/480)-1);check(name+' window',()=>assert(view.segments.includes(first*480)));};
 const loaded=await(await fetch(url+'/api/v1/songs/'+songs[0].id)).json();
 await command('transport.seek',{tick:loaded.durationTicks*.65});await verifyPosition('paused forward seek');
 await command('transport.seek',{tick:loaded.durationTicks*.1});await verifyPosition('paused backward seek');
 await command('transport.play');await command('transport.seek',{tick:loaded.durationTicks*.7});await pause(250);await command('transport.pause');await verifyPosition('playing then pause late');
 await command('transport.load',{id:songs[1].id});await until(async()=> (await page.getByLabel('Song title').inputValue())===songs[1].title);await verifyPosition('second song after stopped late');
 await page.screenshot({path:'web/test-results/final-notation.png'});
 await writeFile('build/final-notation-proof.json',JSON.stringify({...proof,failures},null,2));assert.deepEqual(failures,[]);console.log('PASS complete Flash treble/bass/grand glyphs, snapshot cache, paused forward/back seek, second song reset');
}finally{await browser?.close();if(child?.exitCode===null){const stopped=new Promise(r=>child.once('exit',r));await fetch(url+'/api/v1/shutdown',{method:'POST',headers:{Origin:url}}).catch(()=>child.kill());await stopped;}await rm(data,{recursive:true,force:true});}
