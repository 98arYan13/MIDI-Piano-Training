import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { mkdtemp, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { chromium } from "../web/node_modules/playwright/index.mjs";
import { browserPath } from "./browser-path.mjs";

const data = await mkdtemp(join(tmpdir(), "sr-selection-"));
const url = "http://127.0.0.1:18782";
const service = spawn(process.env.SR_FINGERING_EXECUTABLE ?? "build/native/native/sightreading.exe", ["--port", "18782", "--data-dir", data], { stdio: "pipe" });
let browser, output = "", id = 0;
service.stderr.on("data", s => output += s);
const state = async () => (await (await fetch(url + "/api/v1/status")).json());
const until = async fn => {
  for (let i = 0; i < 150; i++) {
    if (service.exitCode !== null) throw Error(output);
    if (await fn()) return;
    await new Promise(r => setTimeout(r, 100));
  }
  throw Error("Selection condition timeout");
};
const command = async (type, params = {}) => {
  const r = await (await fetch(url + "/api/v1/command", {
    method: "POST", headers: { Origin: url, "Content-Type": "application/json" },
    body: JSON.stringify({ version: 1, id: `selection-${++id}`, type, params }),
  })).json();
  assert.equal(r.ok, true, JSON.stringify(r));
};
try {
  await until(async () => { try { return Boolean(await state()); } catch { return false; } });
  browser = await chromium.launch({ executablePath: browserPath(), headless: true });
  const page = await browser.newPage({ viewport: { width: 1500, height: 1100 } });
  const errors = [];
  page.on("pageerror", e => errors.push(e.message));
  await page.goto(url + "/#play");
  await page.getByRole("button", { name: /Mary Had a Little Lamb.*sec/ }).click();
  const fingerings = page.getByLabel("Hide fingerings", { exact: true });
  assert.equal(await fingerings.isChecked(), true);
  assert.equal(await page.getByLabel("Hide passage controls", { exact: true }).isChecked(), true);
  await page.getByLabel("Hide passage controls", { exact: true }).click();
  assert.equal(await page.getByLabel("Fingering tips", { exact: true }).isVisible(), false);
  assert.equal(await page.getByLabel("Repeat selected part", { exact: true }).isVisible(), true);
  await fingerings.click();
  assert.equal(await page.getByLabel("Fingering tips", { exact: true }).isVisible(), true);
  const repeat = page.getByLabel("Repeat selected part", { exact: true });
  await repeat.waitFor({ timeout: 3000 });
  const p = (await state()).transport;
  const song = await (await fetch(url + "/api/v1/songs/" + p.songId)).json();
  const rail = page.locator(".selection-overview");
  await rail.scrollIntoViewIfNeeded();
  const box = await rail.boundingBox();
  const dragTo = async (name, fraction) => {
    const handle = page.getByRole("slider", { name, exact: true });
    const h = await handle.boundingBox();
    await page.mouse.move(h.x + h.width / 2, h.y + 12);
    await page.mouse.down();
    await page.mouse.move(box.x + box.width * fraction, h.y + 12, { steps: 8 });
    await page.mouse.up();
  };
  await dragTo("Selection end", .2);
  await until(async () => (await state()).transport.config.b < song.durationTicks * .25);
  await dragTo("Selection start", .05);
  await until(async () => (await state()).transport.config.a > 0);
  await repeat.click();
  await until(async () => (await state()).transport.config.loop);
  await command("transport.configure", { speed: 4, practiceEnabled: false, hand: "right", hints: true, accompaniment: "Root" });
  await until(async () => (await state()).pianoReady);
  await command("virtual.noteOn", { source: "selection-live", channel: 0, note: 106, velocity: 70, instance: 1 });
  await command("metronome.start");
  await page.getByRole("button", { name: "Restart", exact: true }).click();
  const config = (await state()).transport.config;
  let wraps = 0, previousState = "playing", rewind, previousReturnTick;
  for (let i = 0; i < 110 && wraps < 2; i++) {
    const s = await state(), t = s.transport;
    assert.ok(t.songTick >= config.a - 1 && t.songTick < config.b + 1);
    if(t.state === "returning") {
      assert.equal(t.return.durationFrames/t.sampleRate,2);
      if(previousState!=="returning") { rewind=t.return;previousReturnTick=t.songTick; }
      else { assert.ok(t.songTick<=previousReturnTick+1);previousReturnTick=t.songTick; }
      assert.equal(t.target.length,0);
      assert.equal(t.hintKeys.length,0);
    }
    if(previousState==="returning"&&t.state==="playing") {
      assert.ok(t.sampleFrame-rewind.startFrame>=rewind.durationFrames);
      wraps++;
    }
    previousState=t.state;
    assert.equal(t.config.speed, 4);
    assert.equal(t.config.hand, "right");
    assert.equal(t.config.accompaniment, "Root");
    assert.equal(s.metronome.running, true);
    assert.ok(s.heldNotes.some(n => n.bus === "live" && n.instance === 1));
    await new Promise(r => setTimeout(r, 100));
  }
  assert.equal(wraps, 2, "selected passage repeats multiple times");
  await command("transport.pause");
  await command("transport.seek",{tick:config.b-1});
  const beforeReturn=(await state()).transport;
  // HTTP acknowledgement precedes the next browser snapshot; compare rewind
  // movement only after the score has rendered this seek position.
  await until(async () => Math.abs(Number(await page.getByLabel("Song position", { exact: true }).inputValue()) - beforeReturn.songTick) < 1);
  await page.evaluate(() => new Promise(resolve => requestAnimationFrame(resolve)));
  const scoreTrack=page.locator(".song-player .staff-track");
  const beforeTransform=await scoreTrack.evaluate(el=>new DOMMatrixReadOnly(getComputedStyle(el).transform).m41);
  await page.getByRole("button",{name:"Return to Start",exact:true}).click();
  await until(async ()=>(await state()).transport.state==="returning");
  await new Promise(r=>setTimeout(r,350));
  const midReturn=(await state()).transport;
  assert.ok(midReturn.songTick<beforeReturn.songTick-1,"rewind movement begins without a two-second delay");
  assert.ok(midReturn.songTick>config.a,"rewind animates instead of instantly seeking");
  assert.equal(midReturn.state,"returning");
  const midTransform=await scoreTrack.evaluate(el=>new DOMMatrixReadOnly(getComputedStyle(el).transform).m41);
  assert.ok(midTransform>beforeTransform+3,"the engraved score itself animates backward immediately");
  await until(async ()=>(await state()).transport.state==="paused");
  assert.ok(Math.abs((await state()).transport.songTick-config.a)<1);
  await command("transport.seek", { tick: config.a });
  const marker = page.getByRole("slider", { name: "Score selection start", exact: true });
  await marker.waitFor({ state: "visible" });
  await marker.focus();
  await page.keyboard.press("ArrowRight");
  await until(async () => (await state()).transport.config.a === config.a + 1);
  const scoreEnd = page.getByRole("slider", { name: "Score selection end", exact: true });
  const endBox = await scoreEnd.boundingBox();
  const beforeDrag = (await state()).transport;
  await page.mouse.move(endBox.x + endBox.width / 2, endBox.y + 70);
  await page.mouse.down();
  await page.mouse.move(endBox.x + endBox.width / 2 - 25, endBox.y + 70, { steps: 8 });
  assert.equal((await state()).transport.config.b, beforeDrag.config.b, "score preview does not commit before release");
  await page.mouse.up();
  await until(async () => (await state()).transport.config.b < beforeDrag.config.b);
  assert.equal((await state()).transport.songTick, beforeDrag.songTick, "moving a boundary does not scrub the playhead");
  await command("transport.configure", { a: config.a + 1, b: config.b });
  await command("transport.configure", { practiceEnabled: true, mode: "wait" });
  await command("transport.restart");
  await until(async () => (await state()).transport.waiting);
  const waiting = (await state()).transport;
  assert.ok(waiting.songTick >= config.a && waiting.songTick < config.b);
  let instance = 100;
  for (const note of waiting.target)
    await command("virtual.noteOn", { source: "selection-answer", channel: 0, note, velocity: 80, instance: ++instance });
  await until(async () => (await state()).transport.hits > waiting.hits);
  await command("transport.pause");
  await page.setViewportSize({ width: 390, height: 844 });
  await rail.scrollIntoViewIfNeeded();
  assert.ok((await rail.boundingBox()).width < 390);
  await page.screenshot({ path: "web/test-results/play-along-selection-mobile.png", fullPage: true });
  await page.setViewportSize({ width: 1500, height: 1100 });
  await page.locator(".song-player").evaluate(el => window.scrollTo(0, el.getBoundingClientRect().top + window.scrollY - 120));
  await page.locator(".song-player").screenshot({ path: "web/test-results/play-along-selection-player.png" });
  await page.screenshot({ path: "web/test-results/play-along-selection.png", fullPage: true });
  await command("transport.configure",{mode:"continuous",practiceEnabled:false});
  await command("transport.play");
  const beforeHide=(await state()).transport;
  const hide=page.getByLabel("Hide passage controls",{exact:true});
  await hide.click();
  await until(async ()=>!(await state()).transport.config.loop);
  await until(async ()=>await page.locator(".song-player .loop-markers").count()===0);
  assert.equal(await page.locator(".selection-controls").count(),0);
  assert.equal(await page.locator(".selection-overview").count(),0);
  const hiddenState=await state();
  assert.equal(hiddenState.transport.config.a,beforeHide.config.a);
  assert.equal(hiddenState.transport.config.b,beforeHide.config.b);
  assert.equal(hiddenState.transport.state,"playing");
  assert.equal(hiddenState.metronome.running,true);
  assert.ok(hiddenState.heldNotes.some(n=>n.bus==="live"&&n.instance===1));
  await page.locator(".song-player").screenshot({path:"web/test-results/play-along-selection-hidden.png"});
  await hide.click();
  await until(async ()=>await page.locator(".selection-overview").count()===1);
  assert.equal((await state()).transport.config.loop,false,"showing controls does not re-enable repeat");
  await command("transport.pause");
  await command("transport.configure",{loop:true});
  await until(() => repeat.isChecked());
  await repeat.click();
  await until(async () => !(await state()).transport.config.loop);
  await command("transport.restart");
  await until(async () => (await state()).transport.songTick < config.a);
  assert.deepEqual(errors, []);
  console.log("PASS draggable passage selection, repeated native loops, wait/hints, settings, live/click ownership and mobile controls");
} finally {
  if (browser) await browser.close();
  if (service.exitCode === null) { service.kill(); await new Promise(r => service.once("exit", r)); }
  await rm(data, { recursive: true, force: true });
}
