import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { mkdir, mkdtemp, rm } from 'node:fs/promises';
import { join } from 'node:path';
import { chromium } from '../web/node_modules/playwright/index.mjs';
import { browserPath } from './browser-path.mjs';
import { DatabaseSync } from 'node:sqlite';

await mkdir('build/test-data', { recursive: true });
const data = await mkdtemp(join(process.cwd(), 'build/test-data/session-'));
const url = 'http://127.0.0.1:18792';
let child, browser;
const errors = [];
const status = async () => (await fetch(url + '/api/v1/status')).json();
async function command(type, params = {}) {
  const reply = await (await fetch(url + '/api/v1/command', {
    method: 'POST', headers: { 'Content-Type': 'application/json', Origin: url },
    body: JSON.stringify({ version: 1, id: 'session-test', type, params }),
  })).json();
  assert.equal(reply.ok, true, JSON.stringify(reply.error));
  return reply.result;
}
async function start() {
  child = spawn(process.env.SR_FINGERING_EXECUTABLE ?? 'build/native/native/sightreading.exe', ['--port', '18792', '--data-dir', data, '--no-tray'], { stdio: 'pipe' });
  child.stderr.on('data', chunk => errors.push(chunk.toString()));
  for (let i = 0; i < 150; i++) {
    try { if ((await fetch(url + '/api/v1/status')).ok) return; } catch {}
    await new Promise(r => setTimeout(r, 100));
  }
  throw Error('Service did not start: ' + errors.join('\n'));
}
async function stop() {
  if (child.exitCode !== null) { child = null; return; }
  const exited = new Promise(r => child.once('exit', r));
  await fetch(url + '/api/v1/shutdown', { method: 'POST', headers: { Origin: url } });
  await exited;
  child = null;
}
async function wait(check) {
  for (let i = 0; i < 100; i++) {
    const s = await status();
    if (check(s)) return s;
    await new Promise(r => setTimeout(r, 100));
  }
  assert.fail('State did not settle');
}
try {
  await start();
  const defaults = await status();
  assert.equal(defaults.preferences.route, 'staff');
  const songs = await (await fetch(url + '/api/v1/songs')).json();
  const song = songs.find(s => s.id === 'starter-ode') ?? songs[0];
  await command('preferences.configure', { route: 'play', sidebarHidden: true, keyboard: { size: 61, labels: false, velocity: 76, octave: 3 } });
  await command('audio.settings', { volume: .43, velocityCurve: 1.3 });
  await command('metronome.config', { tempo: 83, sound: 'rim' });
  await command('practice.configure', { staff: 'grand', key: 'G' });
  await command('ear.configure', { tempo: 87 });
  await command('flash.configure', { staff: 'bass' });
  await command('transport.load', { id: song.id });
  await command('transport.configure', { speed: .75, spacing: 160, hand: 'left', hints: true });
  await command('transport.seek', { tick: song.ppq * 2 });
  const saved = await status();
  await stop();
  await start();
  const restored = await status();
  for (const field of ['preferences', 'settings']) assert.deepEqual(restored[field], saved[field]);
  for (const field of ['practice', 'ear', 'flash']) assert.deepEqual(restored[field].config, saved[field].config);
  assert.deepEqual(restored.metronome.settings, saved.metronome.settings);
  assert.equal(restored.transport.songId, song.id);
  assert.equal(restored.transport.state, 'paused');
  assert.ok(Math.abs(restored.transport.songTick - saved.transport.songTick) < .001);
  assert.deepEqual(restored.transport.config, saved.transport.config);

  browser = await chromium.launch({ executablePath: browserPath(), headless: true });
  const page = await browser.newPage({ viewport: { width: 1440, height: 1000 } });
  page.on('pageerror', e => errors.push(e.message));
  await page.goto(url);
  await page.getByRole('heading', { name: 'Play Along', exact: true }).waitFor();
  await page.getByLabel('Song title', { exact: true }).waitFor();
  assert.equal(await page.getByLabel('Song title', { exact: true }).inputValue(), song.title);
  assert.equal(await page.getByLabel('Keyboard size').inputValue(), '61');
  await page.getByText('Keyboard settings', { exact: true }).click();
  assert.equal(await page.getByLabel('Note labels', { exact: true }).isChecked(), false);
  await page.getByLabel('Keyboard size').selectOption('49');
  await wait(s => s.preferences.keyboard.size === 49);
  await page.getByRole('button', { name: 'Show sidebar', exact: true }).click();
  await page.getByRole('link', { name: /Ear Training/ }).click();
  await wait(s => s.preferences.route === 'ear' && !s.preferences.sidebarHidden);
  await page.goto(url);
  await page.getByRole('heading', { name: 'Ear Training', exact: true }).waitFor();
  assert.equal(await page.getByLabel('Keyboard size').inputValue(), '49');
  // Invalid preferences are rejected atomically without losing valid choices.
  const invalid = await (await fetch(url + '/api/v1/command', {
    method: 'POST', headers: { 'Content-Type': 'application/json', Origin: url },
    body: JSON.stringify({ version: 1, id: 'invalid', type: 'preferences.configure', params: { route: 'play', keyboard: { size: 999 } } }),
  })).json();
  assert.equal(invalid.ok, false);
  assert.equal((await status()).preferences.route, 'ear');
  assert.equal((await status()).preferences.keyboard.size, 49);

  // Native checkpoints follow actual playback while another tab is open.
  await command('transport.configure', { mode: 'continuous', hand: 'both', speed: 1 });
  // Locked SQLite checkpoints must not block the native playback/scoring pump.
  const locked = new DatabaseSync(join(data, 'practice.sqlite'));
  locked.exec('BEGIN IMMEDIATE');
  try {
    const started = performance.now();
    await command('transport.play');
    assert.ok(performance.now() - started < 1000, 'A locked checkpoint delayed the playback command');
    const initialNotes = new Set((await status()).heldNotes.filter(n => n.bus === 'playback').map(n => n.instance));
    let newAttack = false;
    for (let i = 0; i < 15; i++) {
      await new Promise(r => setTimeout(r, 100));
      const playing = await status();
      if (i >= 6 && playing.heldNotes.some(n => n.bus === 'playback' && n.held && !initialNotes.has(n.instance)))
        newAttack = true;
    }
    assert.ok(newAttack, 'No new native playback note was scheduled while SQLite was locked');
  } finally { locked.exec('ROLLBACK'); locked.close(); }
  await wait(s => s.transport.songTick > saved.transport.songTick + song.ppq);
  await stop();
  await start();
  const advanced = await status();
  assert.equal(advanced.transport.state, 'paused');
  assert.ok(advanced.transport.songTick > saved.transport.songTick + song.ppq);

  await wait(s => s.pianoReady && s.audioReady);
  await command('metronome.start');
  await command('virtual.noteOn', { source: 'persistence-test', note: 60, velocity: 80, instance: 901 });
  await wait(s => s.heldNotes.some(n => n.instance === 901));
  await page.goto(url);
  await page.getByRole('button', { name: 'App settings', exact: true }).click();
  await mkdir('docs/verification', { recursive: true });
  await page.screenshot({ path: 'docs/verification/session-settings-desktop.png' });
  await page.setViewportSize({ width: 390, height: 844 });
  await page.screenshot({ path: 'docs/verification/session-settings-mobile.png' });
  assert.ok(await page.getByRole('button', { name: 'Reset all settings to defaults', exact: true }).isVisible());
  await page.getByRole('button', { name: 'Reset all settings to defaults', exact: true }).click();
  const reset = await wait(s => s.preferences.route === 'staff' && s.settings.volume === defaults.settings.volume);
  assert.equal(reset.metronome.running, true);
  assert.ok(reset.heldNotes.some(n => n.instance === 901 && n.held));
  assert.deepEqual(reset.preferences, defaults.preferences);
  assert.deepEqual(reset.practice.config, defaults.practice.config);
  assert.deepEqual(reset.ear.config, defaults.ear.config);
  assert.deepEqual(reset.flash.config, defaults.flash.config);
  assert.deepEqual(reset.metronome.settings, defaults.metronome.settings);
  assert.equal(reset.transport.songId, song.id);
  assert.ok(Math.abs(reset.transport.songTick - advanced.transport.songTick) < .001);
  assert.equal(reset.transport.config.speed, 1);
  assert.equal(reset.transport.config.hand, 'both');
  assert.equal((await (await fetch(url + '/api/v1/songs')).json()).length, songs.length);
  await command('virtual.noteOff', { source: 'persistence-test', note: 60, instance: 901 });
  await stop();
  await start();
  const resetRestored = await status();
  assert.deepEqual(resetRestored.preferences, defaults.preferences);
  assert.deepEqual(resetRestored.settings, defaults.settings);
  assert.deepEqual(resetRestored.metronome.settings, defaults.metronome.settings);

  // A deleted last song must not prevent startup or return to a trashed song.
  await fetch(`${url}/api/v1/songs/${song.id}`, { method: 'DELETE', headers: { Origin: url } });
  await stop();
  await start();
  assert.equal((await status()).transport.songId, '');
  assert.deepEqual(errors, []);
  console.log('Session persistence: settings, tab reload/restart, paused song position, playback checkpoints, safe reset and missing song passed.');
} finally {
  await browser?.close();
  if (child) await stop().catch(async () => {
    if (child?.exitCode === null) {
      const exited = new Promise(r => child.once('exit', r));
      child.kill();
      await exited;
    }
  });
  await rm(data, { recursive: true, force: true, maxRetries: 10, retryDelay: 100 });
}
