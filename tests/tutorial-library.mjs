import assert from 'node:assert/strict';
import {spawn} from 'node:child_process';
import {mkdtemp, rm, readFile} from 'node:fs/promises';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {chromium} from '../web/node_modules/playwright/index.mjs';
import {browserPath} from './browser-path.mjs';
const data = await mkdtemp(join(tmpdir(), 'sr-tutorials-'));
const url = 'http://127.0.0.1:18784';
const child = spawn('build/native/native/sightreading.exe', ['--port','18784','--data-dir',data], {stdio:'pipe'});
let browser, output = '', sequence = 0;
child.stderr.on('data', chunk => output += chunk);
const command = async (type, params = {}) => {
  const result = await (await fetch(url+'/api/v1/command', {method:'POST',headers:{Origin:url,'Content-Type':'application/json'},body:JSON.stringify({version:1,id:++sequence,type,params})})).json();
  assert.equal(result.ok,true,JSON.stringify(result));
  return result;
};
try {
  let ready = false;
  for(let i=0;i<200;i++) {
    try { ready = (await fetch(url+'/api/v1/status')).ok; } catch {}
    if(ready) break;
    if(child.exitCode !== null) throw Error(output);
    await new Promise(resolve=>setTimeout(resolve,100));
  }
  assert.ok(ready,output);
  const songs = await (await fetch(url+'/api/v1/songs')).json();
  const lessons = songs.filter(song=>song.tutorial);
  assert.ok(lessons.length >= 70, `Expected substantial tutorial collection, found ${lessons.length}`);
  const catalog = JSON.parse(await readFile('assets/music/tutorials.json','utf8'));
  assert.equal(lessons.length,catalog.length);
  for(const entry of catalog) {
    const song = lessons.find(song=>song.id===entry.id);
    assert.ok(song,entry.id);
    assert.deepEqual(song.tutorial,entry.tutorial);
    assert.ok(song.tutorial.steps.length>=3);
    assert.ok(song.tutorial.goal.length>20);
    const original = await fetch(url+'/api/v1/songs/'+entry.id+'/original');
    assert.equal(original.status,200);
    assert.deepEqual(Buffer.from(await original.arrayBuffer()),await readFile('assets/music/'+entry.file));
    await command('transport.load',{id:entry.id});
    const events = await (await fetch(url+`/api/v1/songs/${entry.id}/events?fromTick=0&toTick=100000`)).json();
    assert.ok(events.notes.length>0,entry.id);
  }
  browser = await chromium.launch({executablePath:browserPath(),headless:true});
  const page = await browser.newPage({viewport:{width:1440,height:1100}});
  await page.goto(url+'/#play');
  await page.getByLabel('Search MIDI library').fill('La Candeur');
  await page.locator('.song-list .song-row').first().waitFor();
  assert.equal(await page.locator('.song-list .song-row').count(),1);
  await page.locator('.song-list .song-row button').first().click();
  const guide = page.getByRole('region',{name:'Lesson guide'});
  await guide.waitFor();
  assert.ok((await guide.innerText()).includes('La Candeur'));
  assert.ok(await guide.getByRole('link',{name:'Source / score'}).getAttribute('href'));
  await command('metronome.start');
  await command('virtual.noteOn',{source:'tutorial-test',channel:0,note:60,velocity:90,instance:777});
  await command('transport.play');
  await new Promise(resolve=>setTimeout(resolve,250));
  await command('transport.pause');
  await command('transport.seek',{tick:0});
  const state = await (await fetch(url+'/api/v1/status')).json();
  assert.equal(state.metronome.running,true);
  assert.ok(state.heldNotes.some(note=>note.note===60&&note.held));
  await command('virtual.noteOff',{source:'tutorial-test',channel:0,note:60,instance:777});
  await page.getByLabel('Search MIDI library').fill('');
  await page.getByLabel('Lesson difficulty').selectOption('Beginner');
  assert.ok(await page.locator('.song-list .song-row').count()>0);
  assert.ok((await page.locator('.song-list').innerText()).includes('Beginner'));
  await page.getByLabel('Search MIDI library').fill('nothing-matches-this');
  await page.getByText('No matching pieces. Try another search or difficulty.').waitFor();
  await page.getByLabel('Search MIDI library').fill('');
  await page.getByLabel('Lesson difficulty').selectOption('All');
  await page.setViewportSize({width:390,height:844});
  assert.equal(await page.getByRole('link',{name:/Download MIDI:/}).count(),songs.length);
  await page.screenshot({path:'docs/verification/tutorial-library-mobile.png',fullPage:true});
  await page.setViewportSize({width:1440,height:1100});
  await page.screenshot({path:'docs/verification/tutorial-library-desktop.png',fullPage:true});
  console.log(`PASS ${lessons.length} tutorials: native imports, original downloads, event windows, load/play/pause/seek isolation, search, difficulty, guides, mobile`);
} finally {
  await browser?.close();
  if(child.exitCode===null) { const exited = new Promise(resolve=>child.once('exit',resolve)); child.kill(); await exited; }
  await rm(data,{recursive:true,force:true});
}
