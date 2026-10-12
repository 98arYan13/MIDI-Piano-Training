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
