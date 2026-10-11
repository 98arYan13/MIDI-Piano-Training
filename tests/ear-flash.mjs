import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { mkdtemp, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { chromium } from "../web/node_modules/playwright/index.mjs";
const dir = await mkdtemp(join(tmpdir(), "sr-ear-")),
  port = 18777,
  url = `http://127.0.0.1:${port}`;
let child,
  browser,
  id = 0,
  output = "";
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const state = async () => await (await fetch(url + "/api/v1/status")).json();
const until = async (fn) => {
  for (let n = 0; n < 150; n++) {
    if (await fn()) return;
    await sleep(100);
  }
  throw Error("Ear/Flash timeout " + output);
};
const command = async (type, params = {}) =>
  await (
    await fetch(url + "/api/v1/command", {
      method: "POST",
      headers: { Origin: url, "Content-Type": "application/json" },
      body: JSON.stringify({ version: 1, id: ++id, type, params }),
    })
  ).json();
const ok = async (type, params = {}) => {
  const r = await command(type, params);
  assert.equal(r.ok, true, JSON.stringify(r));
  return r.result;
};
const launch = () => {
  child = spawn(
    "build/native/native/sightreading.exe",
    ["--port", String(port), "--data-dir", dir],
    { stdio: "pipe" },
  );
  child.stdout.on("data", (x) => (output += x));
  child.stderr.on("data", (x) => (output += x));
};
const stop = async () => {
  if (child?.exitCode === null) {
    const wait = new Promise((r) => child.once("exit", r));
    child.kill();
    await wait;
  }
};
try {
  launch();
  await until(async () => {
    try {
      return (await state()).pianoReady;
    } catch {
      return false;
    }
  });
  browser = await chromium.launch({
    headless: true,
    executablePath:
      "C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe",
  });
  const page = await browser.newPage({
    viewport: { width: 1365, height: 900 },
  });
  const errors = [];
  page.on("pageerror", (e) => errors.push(String(e)));
  await page.goto(url + "/#ear");
  await page.getByRole("button", { name: "Play root", exact: true }).waitFor();
  await ok("metronome.start");
  await ok("virtual.noteOn", {
    source: "ear-test",
    channel: 0,
    note: 60,
    velocity: 90,
    instance: 700,
  });
  await until(async () =>
    (await state()).heldNotes.some((n) => n.instance === 700),
  );
  const live = async () => {
    const s = await state();
    assert(s.heldNotes.some((n) => n.instance === 700 && n.bus === "live"));
    assert(s.metronome.running);
  };
  for (const interval of [
    "m2",
    "M2",
    "m3",
    "M3",
    "P4",
    "TT",
    "P5",
    "m6",
    "M6",
    "m7",
    "M7",
    "P8",
  ])
    for (const direction of ["up", "down"]) {
      const s = await ok("ear.configure", {
        mode: "interval",
        intervals: [interval],
        directions: [direction],
        tempo: 300,
        root: 60,
        transpose: 2,
        playOrder: "forward",
      });
      assert.equal(s.target.root, 62);
      assert.equal(s.target.interval, interval);
      await live();
    }
  for (const playOrder of ["forward", "reverse", "harmonic"]) {
    await ok("ear.configure", { playOrder });
    let s = await ok("ear.replay");
    assert(s.answerLocked);
    assert.equal((await command("ear.answer", { interval: "P8" })).ok, false);
    assert.equal(
      playOrder === "harmonic"
        ? s.notes[0].startFrame === s.notes[1].startFrame
        : s.notes[0].startFrame < s.notes[1].startFrame,
      true,
    );
    await ok("ear.pause");
    assert((await state()).ear.paused);
    await ok("ear.play");
    await ok("ear.stop");
    await live();
  }
  await ok("ear.configure", { playOrder: "forward" });
  assert.equal((await ok("ear.root")).notes.length, 1);
  assert.equal((await ok("ear.interval")).notes.length, 1);
  assert.equal((await ok("ear.motif")).notes.length, 4);
  await ok("ear.stop");
  assert.equal((await command("ear.configure", { intervals: [] })).ok, false);
  await ok("ear.autoplay", { enabled: true });
  assert((await state()).ear.autoplay);
  await ok("ear.skip");
  await ok("ear.autoplay", { enabled: false });
  await live();
  for (let groups = 2; groups <= 8; groups++)
    for (let pitches = 1; pitches <= 4; pitches++)
      for (const melodyDirection of ["ascending", "descending", "any"]) {
        const s = await ok("ear.configure", {
          mode: "melody",
          groups,
          pitches,
          melodyDirection,
          playOrder: "forward",
          range: { min: 48, max: 84 },
        });
        assert.equal(s.target.groups.length, groups);
        assert(s.target.groups.every((g) => g.length === pitches));
      }
  let s = await ok("ear.configure", {
    groups: 2,
    pitches: 1,
    continuous: true,
    tempo: 300,
  });
  for (const pitches of s.target.groups) await ok("ear.answer", { pitches });
  assert((await state()).ear.playing);
  await ok("ear.stop");
  await ok("ear.configure", { continuous: false });
  for (const root of ["C", "D", "E", "F", "G", "A", "B"])
    for (let distance = 1; distance <= 7; distance++) {
      s = await ok("flash.configure", {
        mode: "noteMath",
        roots: [root],
        distances: [distance],
      });
      assert.equal(
        s.target.answer,
        "CDEFGAB"[("CDEFGAB".indexOf(root) + distance - 1) % 7],
      );
      await ok("flash.answer", { answer: s.target.answer });
    }
  for (const key of [
    "C",
    "G",
    "D",
    "A",
    "E",
    "B",
    "F",
    "Bb",
    "Eb",
    "Ab",
    "Db",
    "Gb",
  ])
    for (let inversion = 0; inversion < 3; inversion++) {
      s = await ok("flash.configure", {
        mode: "chord",
        keys: [key],
        inversions: [inversion],
      });
      assert.equal(s.target.inversion, inversion);
      assert.equal(s.target.notes.length, 3);
      await ok("flash.answer", { answer: s.target.answer });
    }
  assert.equal((await command("flash.configure", { keys: [] })).ok, false);
  await live();
  await ok("ear.configure", {
    mode: "interval",
    intervals: ["M3"],
    directions: ["up"],
    playOrder: "forward",
    root: 60,
    transpose: 0,
  });
  await page.getByRole("button", { name: "Play root", exact: true }).click();
  await page.getByRole("button", { name: "Stop example", exact: true }).click();
  await live();
  await page.getByLabel("Exercise", { exact: true }).selectOption("melody");
  await page.getByRole("button", { name: "New melody", exact: true }).click();
  await page
    .getByRole("button", { name: "Replay melody", exact: true })
    .click();
  await page.getByRole("button", { name: "Stop example", exact: true }).click();
  await page.getByRole("link", { name: /Flash Cards/ }).click();
  await page.getByLabel("Card type", { exact: true }).selectOption("noteMath");
  await ok("flash.configure", { roots: ["C"], distances: [3] });
  await until(
    async () => (await page.getByText("3rd of C", { exact: true }).count()) > 0,
  );
  const shortcutBefore = (await state()).flash;
  await page.dispatchEvent("body", "keydown", {
    key: "3",
    code: "Digit3",
    shiftKey: true,
  });
  await sleep(150);
  const shortcutAfter = (await state()).flash;
  assert.equal(
    shortcutAfter.answered,
    false,
    "Shift-modified digit must not answer",
  );
  assert.equal(shortcutAfter.hits, shortcutBefore.hits);
  assert.equal(shortcutAfter.misses, shortcutBefore.misses);
  await page.keyboard.press("3");
  await until(async () => (await state()).flash.answered);
  await live();
  await page.keyboard.press("m");
  await page.getByRole("heading", { name: "Metronome", exact: true }).waitFor();
  await page.getByRole("button", { name: "Close settings" }).click();
  await page.getByLabel("Card type", { exact: true }).selectOption("chord");
  await page
    .getByRole("img", { name: "Chord identification notation" })
    .waitFor();
  await page.setViewportSize({ width: 390, height: 844 });
  await page.screenshot({ path: "build/task-7-mobile.png", fullPage: true });
  await page.reload();
  await page.getByLabel("Card type", { exact: true }).waitFor();
  await live();
  assert.deepEqual(errors, []);
  const saved = (await state()).flash.config;
  await browser.close();
  browser = null;
  await stop();
  launch();
  await until(async () => {
    try {
      return (await state()).pianoReady;
    } catch {
      return false;
    }
  });
  assert.deepEqual((await state()).flash.config, saved);
  assert.equal((await state()).metronome.running, false);
  assert.equal((await state()).ear.playing, false);
  console.log(
    "PASS Ear/Flash API all qualities/directions/melody sizes/key inversions/answers, native examples/autoplay/held Live/click isolation, Edge UI/mobile/shortcuts/reload/persistence",
  );
} finally {
  await browser?.close();
  await stop();
  await rm(dir, { recursive: true, force: true });
}
