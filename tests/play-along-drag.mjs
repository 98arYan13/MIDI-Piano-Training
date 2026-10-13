import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { mkdtemp, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { chromium } from "../web/node_modules/playwright/index.mjs";
import { browserPath } from "./browser-path.mjs";

const data = await mkdtemp(join(tmpdir(), "sr-song-drag-"));
const url = "http://127.0.0.1:18779";
const service = spawn("build/native/native/sightreading.exe", ["--port", "18779", "--data-dir", data], { stdio: "pipe" });
let browser, output = "", id = 0;
service.stderr.on("data", s => output += s);
const state = async () => (await (await fetch(url + "/api/v1/status")).json()).transport;
const until = async fn => {
  for (let i = 0; i < 150; i++) {
    if (service.exitCode !== null) throw Error(output);
    if (await fn()) return;
    await new Promise(r => setTimeout(r, 100));
  }
  throw Error("Play Along drag condition timeout");
};
const command = async (type, params = {}) => {
  const r = await (await fetch(url + "/api/v1/command", {
    method: "POST", headers: { Origin: url, "Content-Type": "application/json" },
    body: JSON.stringify({ version: 1, id: `drag-${++id}`, type, params }),
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
  await page.getByRole("button", { name: /BWV 846.*sec/ }).click();
  const score = page.getByRole("group", { name: /Song practice notation/ });
  await score.locator(".vf-stavenote").first().waitFor();
  await score.scrollIntoViewIfNeeded();
  const box = await score.boundingBox();
  const x = box.x + box.width * .7, y = box.y + 70;
  const spacing = (await state()).config.spacing;
  const beforeShift = await score.locator(".staff-track").evaluate(el => new DOMMatrix(getComputedStyle(el).transform).m41);
  assert.equal(await score.evaluate(el => getComputedStyle(el).cursor), "grab");
  await page.mouse.click(x, y);
  assert.equal((await state()).state, "stopped");

  await page.mouse.move(x, y);
  await page.mouse.down();
  await page.mouse.move(x - spacing * 2, y, { steps: 10 });
  await until(async () => (await score.getAttribute("class")).includes("dragging-staff"));
  assert.notEqual((await state()).state, "playing");
  assert.equal(await score.evaluate(el => getComputedStyle(el).cursor), "grabbing");
  assert.equal((await state()).songSeconds, 0, "preview does not flood native seek commands");
  const shift = await score.locator(".staff-track").evaluate(el => new DOMMatrix(getComputedStyle(el).transform).m41);
  assert.ok(Math.abs(shift - (beforeShift - spacing * 2)) < 2, "preview follows engraved pixels");
  await page.mouse.up();
  await until(async () => (await state()).songSeconds > 0);
  const forwardSeconds = (await state()).songSeconds;
  await until(async () => !(await score.getAttribute("class")).includes("dragging-staff"));

  const dragBy = async dx => {
    await page.mouse.move(x, y); await page.mouse.down();
    await page.mouse.move(x + dx, y, { steps: 10 }); await page.mouse.up();
  };
  await dragBy(spacing);
  await until(async () => (await state()).songSeconds < forwardSeconds);
  await dragBy(10000);
  await until(async () => (await state()).songSeconds === 0);
  const songs = await (await fetch(url + "/api/v1/songs")).json();
  const song = songs.find(s => s.title.startsWith("BWV 846"));
  await dragBy(-10000);
  await until(async () => Math.abs((await state()).songSeconds - song.durationSeconds) < .01);
  await until(async () => Number(await page.getByLabel("Song position", { exact: true }).inputValue()) >= song.durationTicks - 1);
  await command("transport.seek", { seconds: 2 });
  await command("transport.play");
  await until(async () => (await state()).state === "playing");
  await page.getByRole("button", { name: "Pause", exact: true }).waitFor();
  await dragBy(-spacing);
  await until(async () => {
    const p = await state();
    return p.state === "paused" && p.songSeconds > 2;
  });
  await until(async () => !(await score.getAttribute("class")).includes("dragging-staff"));
  await page.screenshot({ path: "web/test-results/play-along-drag.png", fullPage: true });
  // Wait and hints share the native hand-filtered target; no metronome commands.
  const waitMode = page.getByLabel("Wait mode", { exact: true });
  const hints = page.getByLabel("Hint mode", { exact: true });
  await waitMode.click();
  await hints.click();
  await until(async () => (await state()).config.mode === "wait" && (await state()).config.hints);
  await page.getByRole("button", { name: /Mary Had a Little Lamb.*sec/ }).click();
  await until(async () => (await state()).title === "Mary Had a Little Lamb");
  const mary = await (await fetch(url + "/api/v1/songs/" + (await state()).songId)).json();
  assert.equal(mary.tracks[1].practice, false);
  assert.equal(mary.tracks[1].backing, true);
  const toggleHand = async (button, expected) => {
    await page.getByRole("button", { name: button, exact: true }).click();
    await until(async () => (await state()).config.hand === expected);
  };
  for (const hand of ["left", "right", "both"]) {
    await page.getByRole("button", { name: "Stop", exact: true }).click();
    await until(async () => (await state()).state === "stopped");
    const current = (await state()).config.hand;
    if (current === "left") await toggleHand("Right hand", "both");
    if (current === "right") await toggleHand("Left hand", "both");
    if (hand === "left") await toggleHand("Right hand", "left");
    if (hand === "right") await toggleHand("Left hand", "right");
    await until(async () => (await state()).config.hand === hand);
    const missesBeforePlay = (await state()).misses;
    await page.getByRole("button", { name: "Play", exact: true }).click();
    await until(async () => (await state()).waiting);
    const frozen = await state();
    assert.ok(frozen.target.length > 0);
    assert.equal(frozen.misses, missesBeforePlay, "waiting is not a missed deadline");
    await new Promise(r => setTimeout(r, 250));
    assert.equal((await state()).songTick, frozen.songTick);
    await until(async () => (await page.locator(".piano-key[data-hint]").count()) === frozen.target.length);
    for (const key of frozen.hintKeys) {
      assert.ok(hand === "both" || key.hand === hand);
      assert.equal(await page.locator(`.piano-key[data-midi="${key.midiNote}"]`).getAttribute("data-hint"), key.hand);
    }
    await command("virtual.noteOn", { source: "wait-test", channel: 0, note: 107, velocity: 90, instance: 9000 });
    assert.equal((await state()).waiting, true);
    await command("virtual.noteOff", { source: "wait-test", channel: 0, note: 107, instance: 9000 });
    let instance = 9100;
    for (const note of frozen.target)
      await command("virtual.noteOn", { source: "wait-test", channel: 0, note, velocity: 90, instance: ++instance });
    await until(async () => (await state()).hits === frozen.hits + 1);
    instance = 9100;
    for (const note of frozen.target)
      await command("virtual.noteOff", { source: "wait-test", channel: 0, note, instance: ++instance });
  }
  await command("transport.stop");
  await command("transport.play");
  await until(async () => (await state()).waiting);
  await hints.click();
  await until(async () => (await page.locator(".piano-key[data-hint]").count()) === 0);
  assert.equal((await state()).waiting, true, "hints off does not turn wait off");
  await hints.click();
  await until(async () => (await page.locator(".piano-key[data-hint]").count()) > 0);
  const waitingPlayerY = await page.locator(".song-player").evaluate(el => el.getBoundingClientRect().top + window.scrollY);
  await waitMode.click();
  await until(async () => (await state()).state === "playing" && !(await state()).waiting);
  await until(async () => await page.locator(".song-workspace .retry:visible").count() === 0);
  const playingPlayerY = await page.locator(".song-player").evaluate(el => el.getBoundingClientRect().top + window.scrollY);
  assert.ok(Math.abs(playingPlayerY - waitingPlayerY) < 1, "score stays in place when waiting message disappears");
  assert.equal((await state()).config.hints, true);
  await page.getByRole("button", { name: "Pause", exact: true }).click();
  await hints.click();
  await until(async () => !(await state()).config.hints && !(await state()).waiting);
  await waitMode.click();
  await command("transport.stop");
  await command("transport.play");
  await until(async () => (await state()).waiting);
  assert.equal((await state()).config.hints, false);
  await hints.click();
  await until(async () => (await page.locator(".piano-key[data-hint]").count()) > 0);
  await page.evaluate(() => window.scrollTo(0, 0));
  await page.screenshot({ path: "web/test-results/play-along-wait-hints.png", fullPage: true });
  await page.setViewportSize({ width: 390, height: 844 });
  const mobileWaitingY = await page.locator(".song-player").evaluate(el => el.getBoundingClientRect().top + window.scrollY);
  await waitMode.click();
  await until(async () => !(await state()).waiting);
  await until(async () => await page.locator(".song-workspace .retry:visible").count() === 0);
  const mobilePlayingY = await page.locator(".song-player").evaluate(el => el.getBoundingClientRect().top + window.scrollY);
  assert.ok(Math.abs(mobilePlayingY - mobileWaitingY) < 1, "mobile score stays in place when waiting clears");
  assert.deepEqual(errors, []);
  console.log("PASS Play Along dragging + wait left/right/both correct input, wrong-key freeze, hint colors and all wait/hint toggle combinations; no metronome tests");
} finally {
  if (browser) await browser.close();
  if (service.exitCode === null) {
    const exited = new Promise(r => service.once("exit", r));
    service.kill(); await exited;
  }
  await rm(data, { recursive: true, force: true });
}
