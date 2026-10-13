import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { mkdtemp, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { chromium } from '../web/node_modules/playwright/index.mjs';
import { browserPath } from './browser-path.mjs';
const data = await mkdtemp(join(tmpdir(), 'sr-focus-'));
const url = 'http://127.0.0.1:18788';
const service = spawn(process.env.SR_FINGERING_EXECUTABLE ?? 'build/native/native/sightreading.exe', ['--port', '18788', '--data-dir', data], { stdio: 'pipe' });
let browser, id = 0;
const state = async () => (await (await fetch(url + '/api/v1/status')).json());
const until = async fn => { for (let i = 0; i < 150; i++) { if (await fn()) return; await new Promise(r => setTimeout(r,100)); } throw Error('Focus condition timeout'); };
const command = async (type, params = {}) => {
  const r = await (await fetch(url + '/api/v1/command', { method: 'POST', headers: { Origin: url, 'Content-Type': 'application/json' }, body: JSON.stringify({ version: 1, id: `focus-${++id}`, type, params }) })).json();
  assert.equal(r.ok, true, JSON.stringify(r));
};
try {
  await until(async () => { try { return (await state()).pianoReady; } catch { return false; } });
  browser = await chromium.launch({ executablePath: browserPath(), headless: true });
  const page = await browser.newPage({ viewport: { width: 1530, height: 1039 } });
  const errors = []; page.on('pageerror', e => errors.push(e.message));
  await page.goto(url + '/#play');
  await page.getByRole('button', { name: /Mary Had a Little Lamb.*sec/ }).click();
  await page.locator('.score-measure .vf-stavenote').first().waitFor();
  const savedSize = (await state()).preferences.keyboard.size;
  for (const width of [787, 1024, 1100]) {
    await page.setViewportSize({ width, height: 1039 });
    await page.waitForTimeout(100);
    const layout = await page.evaluate(() => {
      const score = document.querySelector('.song-player').getBoundingClientRect();
      const piano = document.querySelector('.piano-panel').getBoundingClientRect();
      const library = document.querySelector('.song-library').getBoundingClientRect();
      const keys = document.querySelector('.piano-keys');
      return { gap: piano.top - score.bottom, libraryBelow: library.top >= piano.bottom,
        overflow: document.documentElement.scrollWidth > innerWidth,
        keyCount: keys.querySelectorAll('.piano-key').length,
        statsHidden: getComputedStyle(document.querySelector('.scoreline')).display === 'none' };
    });
    assert(layout.gap >= 0 && layout.gap <= 12, `score/keys stay adjacent at ${width}`);
    assert(layout.libraryBelow && layout.statsHidden && !layout.overflow);
    assert(layout.keyCount < 88, 'automatic keyboard fits compact width');
    assert.equal((await state()).preferences.keyboard.size, savedSize, 'resize preserves native preferences');
  }
  await page.getByText('Keyboard options', { exact: true }).click();
  assert(await page.getByLabel('Keyboard size', { exact: true }).isVisible());
  await page.setViewportSize({ width: 1530, height: 1039 });
  await command('transport.configure', { mode: 'wait', hints: true, hearPractice: false, hand: 'right', spacing: 140, speed: .8 });
  await command('metronome.start');
  await page.getByLabel('Keyboard size', { exact: true }).selectOption('49');
  await page.getByRole('button', { name: 'Hide keyboard', exact: true }).click();
  const before = (await state()).transport;
  await command('virtual.noteOn', { source: 'focus-held', channel: 0, note: 90, velocity: 90, instance: 900 });
  await until(async () => (await state()).heldNotes.some(n => n.instance === 900 && n.held));
  await page.getByRole('button', { name: 'Focus mode', exact: true }).click({ timeout: 5000 });
  await page.getByRole('button', { name: 'Return to Play Along', exact: true }).waitFor();
  assert.equal(await page.locator('.piano-key').count(), 49, 'hidden keyboard appears in focus without changing size');
  for (const selector of ['.global-toolbar', '.side-nav', '.song-library', '.song-settings', '.transport-buttons', '.piano-heading', '.piano-tools']) assert.equal(await page.locator(selector).isVisible(), false, selector);
  assert.deepEqual((await state()).transport.config, before.config);
  assert.equal((await state()).transport.songTick, before.songTick);
  assert((await state()).metronome.running);
  assert((await state()).heldNotes.some(n => n.instance === 900 && n.held), 'entering focus preserves live notes');
  await command('virtual.noteOff', { source: 'focus-held', channel: 0, instance: 900 });
  await page.getByRole('button', { name: 'Play', exact: true }).click();
  await until(async () => (await state()).transport.waiting);
  await until(async () => (await page.locator('.key-hint').count()) > 0);
  const waiting = (await state()).transport;
  for (const n of waiting.target) await page.locator(`[data-midi="${n}"]`).click();
  await until(async () => (await state()).transport.hits > waiting.hits);
  await page.getByRole('button', { name: 'Pause', exact: true }).click();
  await page.getByRole('button', { name: 'Return to Play Along', exact: true }).click();
  await page.getByRole('button', { name: 'Show keyboard', exact: true }).waitFor({ state: 'visible' });
  assert.equal(await page.getByRole('button', { name: 'Show keyboard', exact: true }).isVisible(), true);
  assert.deepEqual((await state()).transport.config, before.config);
  await page.getByRole('button', { name: 'Focus mode', exact: true }).click();
  await page.getByRole('button', { name: 'Return to Play Along', exact: true }).waitFor({ state: 'visible' });
  await page.keyboard.press('Escape');
  await page.getByRole('button', { name: 'Focus mode', exact: true }).waitFor({ state: 'visible' });
  assert.equal(await page.getByRole('button', { name: 'Focus mode', exact: true }).isVisible(), true);
  await page.getByLabel('Hint mode', { exact: true }).click();
  await until(async () => !(await state()).transport.config.hints);
  await page.getByRole('button', { name: 'Focus mode', exact: true }).click();
  await page.getByRole('button', { name: 'Play', exact: true }).click();
  await until(async () => (await state()).transport.waiting);
  assert.equal(await page.locator('.key-hint').count(), 0);
  for (const [width, height] of [[1024,768], [1258,1039], [1366,768], [1920,1080], [2560,1440]]) {
    await page.setViewportSize({ width, height });
    await page.waitForTimeout(100);
    const layout = await page.evaluate(() => {
      const piano = document.querySelector('.piano-panel').getBoundingClientRect();
      return { overflow: document.documentElement.scrollWidth > innerWidth, pianoBottom: piano.bottom, height: innerHeight };
    });
    assert.equal(layout.overflow, false, `desktop focus overflow at ${width}x${height}`);
    assert(layout.pianoBottom <= layout.height, `score and keyboard fit at ${width}x${height}`);
  }
  await page.setViewportSize({ width: 390, height: 844 });
  assert(await page.getByRole('button', { name: 'Return to Play Along', exact: true }).isVisible());
  assert(await page.locator('.piano-scroll').isVisible());
  assert(await page.locator('.engraved-score').isVisible());
  assert(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth));
  assert.deepEqual(errors, []);
  console.log('PASS Focus mode: layout, retained settings/keyboard size/hidden state, native wait scoring, enabled/disabled hints, metronome, return/Escape and mobile');
} finally {
  await browser?.close();
  if (service.exitCode === null) { const done = new Promise(r => service.once('exit', r)); service.kill(); await done; }
  await rm(data, { recursive: true, force: true });
}
