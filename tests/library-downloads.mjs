import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {chromium} from '../web/node_modules/playwright/index.mjs';
import {browserPath} from './browser-path.mjs';
const url=process.env.SR_LIBRARY_URL || 'http://127.0.0.1:8765';
const manifest=JSON.parse(await readFile('assets/music/manifest.json','utf8'));
const songs=await (await fetch(url+'/api/v1/songs')).json();
const browser=await chromium.launch({executablePath:browserPath(),headless:true});
try {
 const page=await browser.newPage({viewport:{width:1301,height:1039}});
 await page.goto(url+'/#play');
 await page.getByRole('button',{name:/^Twinkle Twinkle.*sec/}).click();
 assert.equal(await page.getByLabel('Hear practice part',{exact:true}).isChecked(),true);
 const geometry=await page.evaluate(()=>{
  const rect=selector=>document.querySelector(selector).getBoundingClientRect();
  return {stage:rect('.workspace-stage').width,play:rect('.play-along').width,
    song:rect('.song-workspace').width,staff:rect('.song-workspace > .scrolling-staff').width,
    gap:rect('.piano-panel').top-rect('.workspace-stage').bottom};
 });
 assert.ok(Math.abs(geometry.stage-geometry.play)<3);
 assert.ok(Math.abs(geometry.song-geometry.staff)<3);
 assert.ok(geometry.gap<=14);
 await page.getByRole('button',{name:'Hide sidebar',exact:true}).click();
 assert.equal(await page.getByRole('navigation',{name:'Practice tabs'}).isVisible(),false);
 await page.getByRole('button',{name:'Show sidebar',exact:true}).click();
 for(const entry of manifest){
  assert.ok(songs.some(song=>song.id===entry.id));
  const expected=await readFile('assets/music/'+entry.file);
  const response=await fetch(url+'/api/v1/songs/'+entry.id+'/original');
  assert.equal(response.status,200);
  assert.deepEqual(Buffer.from(await response.arrayBuffer()),expected);
  const event=page.waitForEvent('download');
  await page.getByRole('link',{name:'Download MIDI: '+entry.title,exact:true}).click();
  const download=await event;
  assert.deepEqual(await readFile(await download.path()),expected);
 }
 await page.setViewportSize({width:390,height:844});
 assert.equal(await page.getByRole('link',{name:/Download MIDI:/}).count(),songs.length);
 console.log('PASS all '+manifest.length+' saved library pieces and byte-exact native/browser downloads; desktop/mobile');
}finally{await browser.close();}
