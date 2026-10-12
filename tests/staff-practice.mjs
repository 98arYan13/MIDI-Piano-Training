import { browserPath } from "./browser-path.mjs";
import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { mkdtemp, rm, mkdir } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { chromium } from "../web/node_modules/playwright/index.mjs";
import WebSocket from "../web/node_modules/ws/wrapper.mjs";
const data = await mkdtemp(join(tmpdir(), "sr-staff-")),
  port = 18771,
  url = `http://127.0.0.1:${port}`;
let child, browser, other;
let id = 0,
  output = "";
const errors = [];
const wait = (ms) => new Promise((r) => setTimeout(r, ms));
const state = async () => await (await fetch(url + "/api/v1/status")).json();
const until = async (predicate) => {
  for (let i = 0; i < 150; ++i) {
    if (child?.exitCode !== null && child?.exitCode !== undefined)
      throw Error(output);
    if (await predicate()) return;
    await wait(100);
  }
  const s = await state();
  throw Error(
    `Native Staff condition timeout: ${JSON.stringify({ frame: s.sampleFrame, error: s.error, practice: { running: s.practice.running, paused: s.practice.paused, hits: s.practice.hits, misses: s.practice.misses, error: s.practice.error, pressed: s.practice.pressed }, held: s.heldNotes, output })}`,
  );
};
const command = async (type, params = {}) => {
  const request = { version: 1, id: `staff-${++id}`, type, params };
  const reply = await (
    await fetch(url + "/api/v1/command", {
      method: "POST",
      headers: { Origin: url, "Content-Type": "application/json" },
      body: JSON.stringify(request),
    })
  ).json();
  assert.equal(reply.id, request.id);
  return reply;
};
const ok = async (type, params = {}) => {
  const r = await command(type, params);
  assert.equal(r.ok, true, JSON.stringify(r));
  return r.result;
};
const launch = () => {
  child = spawn(
    "build/native/native/sightreading.exe",
    ["--port", String(port), "--data-dir", data],
    { stdio: "pipe" },
  );
  child.stdout.on("data", (b) => (output += b));
  child.stderr.on("data", (b) => (output += b));
};
const stop = async () => {
  if (child?.exitCode === null) {
    const exited = new Promise((r) => child.once("exit", r));
    child.kill();
    await exited;
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
  const initial = (await state()).practice.config;
  assert.equal(initial.staff, "treble");
  assert.equal(initial.generator, "random");
  assert.equal(initial.mode, "wait");
  assert.equal(initial.generatorOptions.hands, 2);
  await ok("practice.configure", {
    range: { min: 60, max: 60 },
    smoothness: 0,
  });
  await ok("practice.start");
  await ok("practice.answer", {
    action: "on",
    source: "staff-api",
    channel: 0,
    note: 61,
    velocity: 90,
    instance: 1,
  });
  await until(async () => (await state()).practice.misses === 1);
  await ok("virtual.noteOn", {
    source: "staff-api",
    channel: 0,
    note: 60,
    velocity: 90,
    instance: 2,
  });
  await wait(100);
  assert.equal((await state()).practice.hits, 0);
  await ok("practice.answer", {
    action: "off",
    source: "staff-api",
    channel: 0,
    instance: 1,
  });
  await ok("virtual.noteOff", { source: "staff-api", channel: 0, instance: 2 });
  await ok("virtual.noteOn", {
    source: "staff-api",
    channel: 0,
    note: 60,
    velocity: 90,
    instance: 3,
  });
  await until(async () => (await state()).practice.hits === 1);
  await ok("metronome.start");
  await until(async () => (await state()).metronome.running);
  await ok("practice.reset");
  await until(async () => (await state()).practice.hits === 0);
  let s = await state();
  assert(s.heldNotes.some((n) => n.instance === 3 && n.held));
  assert(s.metronome.running);
  assert(s.practice.pressed.some((n) => n.instance === 3));
  await ok("practice.stop");
  await ok("virtual.disconnect", { source: "staff-api" });
  await until(async () => (await state()).practice.pressed.length === 0);
  // Invalid config is atomic and leaves canonical state unchanged.
  const before = (await state()).practice.config;
  let bad = await command("practice.configure", {
    generator: "intervals",
    generatorOptions: { intervals: [] },
  });
  assert.equal(bad.ok, false);
  assert.match(bad.error.message, /at least one interval/);
  assert.deepEqual((await state()).practice.config, before);
  await ok("practice.configure", {
    range: { min: 48, max: 84 },
    generator: "random",
    mode: "wait",
    smoothness: 3,
  });
  browser = await chromium.launch({
    executablePath: browserPath(),
    headless: true,
  });
  const page = await browser.newPage({
    viewport: { width: 1440, height: 1050 },
  });
  page.on("pageerror", (e) => errors.push(e.message));
  await page.goto(url);
  await page.getByLabel("Staff view", { exact: true }).waitFor();
  await page.getByLabel("Staff view", { exact: true }).selectOption("grand");
  await until(async () => (await state()).practice.config.staff === "grand");
  await page.locator('.score-gutter svg').waitFor();
  assert.equal(
    await page.locator('.score-gutter svg').count(),
    1,
  );
  assert.equal(await page.locator(".piano-key").count(), 88);
  // Hand controls select native generators, and transport preserves progress while paused.
  await page.getByRole("button", { name: "Left hand", exact: true }).click();
  await until(async () => (await state()).practice.config.hand === "right");
  await page
    .getByRole("button", { name: "Start exercise", exact: true })
    .click();
  await until(async () => (await state()).practice.running);
  assert((await state()).practice.notes.every((n) => n.midiNote >= 60));
  await page
    .getByRole("button", { name: "Pause exercise", exact: true })
    .click();
  await until(async () => (await state()).practice.paused);
  const paused = (await state()).practice;
  await wait(250);
  assert.equal((await state()).practice.elapsed, paused.elapsed);
  await page
    .getByRole("button", { name: "Resume exercise", exact: true })
    .click();
  await until(async () => (await state()).practice.running);
  assert.deepEqual((await state()).practice.targets, paused.targets);
  await page.getByRole("button", { name: "Left hand", exact: true }).click();
  await until(async () => (await state()).practice.config.hand === "both");
  await page.getByRole("button", { name: "Right hand", exact: true }).click();
  await until(async () => (await state()).practice.config.hand === "left");
  await page
    .getByRole("button", { name: "Start exercise", exact: true })
    .click();
  await until(async () => (await state()).practice.running);
  assert((await state()).practice.notes.every((n) => n.midiNote < 60));
  await page.getByRole("button", { name: "Right hand", exact: true }).click();
  await until(async () => (await state()).practice.config.hand === "both");
  assert.equal(
    await page
      .locator(".paper-staff")
      .evaluate((e) => getComputedStyle(e).backgroundColor),
    "rgb(255, 255, 255)",
  );
  const geometry = await page.locator(".paper-staff").evaluate((e) => {
    const score = e.getBoundingClientRect(),
      bar = e.querySelector(".staff-playhead").getBoundingClientRect();
    return {
      width: bar.width,
      offset: Math.abs(bar.x + bar.width / 2 - score.x - score.width / 2),
    };
  });
  assert.equal(geometry.width, 2);
  assert(geometry.offset < 1);
  assert.equal(await page.locator('.practice-score button').count(), 0, 'exercise controls never cover the current note');
  assert(
    (await page.locator(".score-measure[data-measure=\"2\"]").count()) === 1,
  );
  await page.locator(".practice-settings summary").click();
  // Native partial edits do not lose independent changes; numeric text stays local until committed.
  const tempo = page.getByLabel("Practice tempo", { exact: true });
  await tempo.fill("2");
  await wait(100);
  assert.equal((await state()).practice.config.tempo, 100);
  await tempo.fill("120");
  await tempo.press("Enter");
  await until(async () => (await state()).practice.config.tempo === 120);
  await page.getByLabel("Minimum MIDI note", { exact: true }).fill("60");
  await page.getByLabel("Minimum MIDI note", { exact: true }).press("Enter");
  await until(async () => (await state()).practice.config.range.min === 60);
  await page.getByLabel("Maximum MIDI note", { exact: true }).fill("60");
  await page.getByLabel("Maximum MIDI note", { exact: true }).press("Enter");
  await until(async () => (await state()).practice.config.range.max === 60);
  await page.getByLabel("Smoothness", { exact: true }).fill("0");
  await until(async () => (await state()).practice.config.smoothness === 0);
  await page
    .getByRole("button", { name: "Start exercise", exact: true })
    .click();
  await until(async () => (await state()).practice.running);
  await page.locator('[data-midi="60"]').click();
  await until(async () => (await state()).practice.hits === 1);
  // Separate browser source emulates unrelated physical ownership at the real boundary.
  other = new WebSocket(url.replace("http", "ws") + "/api/v1/events", {
    origin: url,
  });
  await new Promise((r, j) => {
    other.once("open", r);
    other.once("error", j);
  });
  other.send(
    JSON.stringify({
      version: 1,
      id: "held-owner",
      type: "virtual.noteOn",
      params: {
        source: "other",
        channel: 0,
        note: 60,
        velocity: 90,
        instance: 999,
      },
    }),
  );
  await until(async () =>
    (await state()).heldNotes.some((n) => n.instance === 999 && n.held),
  );
  await page.getByRole("button", { name: "Reset score", exact: true }).click();
  await until(async () => (await state()).practice.hits === 0);
  assert((await state()).heldNotes.some((n) => n.instance === 999 && n.held));
  assert((await state()).metronome.running);
  await page
    .getByRole("button", { name: "Pause exercise", exact: true })
    .click();
  await until(async () => (await state()).practice.paused);
  assert((await state()).heldNotes.some((n) => n.instance === 999 && n.held));
  assert((await state()).metronome.running);
  await page
    .getByRole("button", { name: "Restart exercise", exact: true })
    .click();
  await until(
    async () =>
      (await state()).practice.running && (await state()).practice.index === 0,
  );
  assert((await state()).heldNotes.some((n) => n.instance === 999 && n.held));
  assert((await state()).metronome.running);
  await page.getByRole("link", { name: /Ear Training/ }).click();
  assert((await state()).heldNotes.some((n) => n.instance === 999 && n.held));
  await page.getByRole("link", { name: /Staff/ }).click();
  await page.locator(".practice-settings summary").click();
  await page
    .getByLabel("Practice mode", { exact: true })
    .selectOption("scroll");
  await until(async () => (await state()).practice.config.mode === "scroll");
  await page
    .getByRole("button", { name: "Start exercise", exact: true })
    .click();
  await wait(250);
  const a = await page
    .locator(".staff-track")
    .evaluate((e) => e.style.transform);
  await wait(250);
  assert.notEqual(
    await page.locator(".staff-track").evaluate((e) => e.style.transform),
    a,
  );
  await page
    .getByRole("button", { name: "Stop exercise", exact: true })
    .click();
  await until(async () => !(await state()).practice.running);
  await page
    .getByRole("button", { name: "Start exercise", exact: true })
    .waitFor();
  await wait(100);
  const frozen = await page
    .locator(".staff-track")
    .evaluate((e) => e.style.transform);
  await wait(250);
  assert.equal(
    await page.locator(".staff-track").evaluate((e) => e.style.transform),
    frozen,
  );
  assert((await state()).metronome.running);
  // Every generator family and its visible controls changes native canonical settings.
  await ok("practice.configure", {
    range: { min: 36, max: 96 },
    mode: "wait",
    smoothness: 0,
  });
  for (const generator of [
    "random",
    "triads",
    "sevenths",
    "progression",
    "position",
    "intervals",
  ]) {
    await page.getByLabel("Generator", { exact: true }).selectOption(generator);
    await until(
      async () => (await state()).practice.config.generator === generator,
    );
    if (generator === "random") {
      await page.getByLabel("Simultaneous notes", { exact: true }).fill("5");
      await page
        .getByLabel("Simultaneous notes", { exact: true })
        .press("Enter");
      await page.getByLabel("Chord based", { exact: true }).click();
      await until(
        async () => (await state()).practice.config.generatorOptions.chordBased,
      );
    }
    if (generator === "sevenths") {
      await page.getByLabel("Open voicing", { exact: true }).click();
      await until(
        async () =>
          (await state()).practice.config.generatorOptions.openVoicing,
      );
    }
    if (generator === "progression") {
      for (const v of ["autumnLeaves", "fifties", "circle", "substitution"]) {
        await page.getByLabel("Progression", { exact: true }).selectOption(v);
        await until(
          async () =>
            (await state()).practice.config.generatorOptions.progression === v,
        );
      }
    }
    if (generator === "position") {
      await page.getByLabel("Starting finger", { exact: true }).fill("3");
      await page.getByLabel("Starting finger", { exact: true }).press("Enter");
      await until(
        async () =>
          (await state()).practice.config.generatorOptions.startingFinger === 3,
      );
    }
    if (generator === "intervals") {
      await page.getByLabel("2nd", { exact: true }).click();
      await until(
        async () =>
          !(await state()).practice.config.generatorOptions.intervals.includes(
            2,
          ),
      );
    }
    await page
      .getByRole("button", { name: "Start exercise", exact: true })
      .click();
    await until(async () => (await state()).practice.running);
    await page
      .getByRole("button", { name: "Stop exercise", exact: true })
      .click();
    await until(async () => !(await state()).practice.running);
  }
  await page
    .getByLabel("Staff view", { exact: true })
    .selectOption("chordSymbol");
  await until(
    async () => (await state()).practice.config.staff === "chordSymbol",
  );
  await page.getByLabel("Chord tones", { exact: true }).selectOption("4");
  await page.getByLabel("Multiple keys", { exact: true }).click();
  await page
    .getByRole("group", { name: "Chord keys" })
    .getByLabel("F", { exact: true })
    .click();
  await page.getByLabel("Common notes", { exact: true }).selectOption("2");
  await until(async () => {
    let o = (await state()).practice.config.generatorOptions;
    return o.tones === 4 && o.keys.includes("F") && o.commonNotes === 2;
  });
  for (const scale of [
    "major",
    "naturalMinor",
    "harmonicMinor",
    "melodicMinor",
  ]) {
    await page.getByLabel("Scale", { exact: true }).selectOption(scale);
    await until(
      async () =>
        (await state()).practice.config.generatorOptions.scale === scale,
    );
  }
  await page
    .getByRole("button", { name: "Start exercise", exact: true })
    .click();
  await until(async () => (await state()).practice.running);
  assert(
    (await page.getByLabel("Chord symbol target").textContent()).length > 0,
  );
  await page
    .getByRole("button", { name: "Stop exercise", exact: true })
    .click();
  // Actual native enharmonic spelling reaches the renderer: Gb major B3 sounds as Cb4.
  await ok("practice.configure", {
    staff: "treble",
    generator: "random",
    key: "Gb",
    range: { min: 59, max: 59 },
    generatorOptions: { notes: 1, chordBased: false, scale: "major" },
    smoothness: 0,
  });
  await ok("practice.start");
  await until(
    async () =>
      (await page.locator(".score-measure .vf-stavenote").count()) > 0,
  );
  assert.equal(
    await page
      .locator(".score-measure svg")
      .first()
      .evaluate((e) => getComputedStyle(e).overflow),
    "visible",
  );
  assert.equal(
    await page
      .locator(".score-gutter")
      .evaluate((e) => getComputedStyle(e).boxShadow),
    "none",
  );
  const note = (await state()).practice.notes[0];
  assert.equal(note.midiNote, 59);
  assert.deepEqual(note.spelling, { letter: "c", accidental: "b", octave: 4 });
  // Cb4 is carried by the Gb key signature, not repeated at every note.
  await until(async () => (await page.locator('.score-gutter .vf-keysignature').count()) === 1);
  assert.equal(await page.locator('.score-measure .vf-accidental').count(), 0);
  assert.equal(await page.locator('.score-measure .vf-notehead text').first().getAttribute('y'), '120');
  await ok("practice.configure", { key: "C" });
  await ok("practice.start");
  await until(async () => (await page.locator('.score-measure .vf-notehead text').first().getAttribute('y')) === '125');
  assert.deepEqual((await state()).practice.notes[0].spelling, {
    letter: "b", accidental: "n", octave: 3,
  });
  assert.equal(await page.locator('.score-measure .vf-accidental').count(), 0);
  await page
    .getByRole("button", { name: "Exercise fullscreen", exact: true })
    .click();
  assert(await page.evaluate(() => !!document.fullscreenElement));
  await page.evaluate(() => document.exitFullscreen());
  await mkdir("web/test-results", { recursive: true });
  await page.screenshot({
    path: "web/test-results/staff-practice.png",
    fullPage: true,
  });
  await page.setViewportSize({ width: 390, height: 844 });
  const mobile = await page.locator(".paper-staff").evaluate((e) => {
    const score = e.getBoundingClientRect(),
      bar = e.querySelector(".staff-playhead").getBoundingClientRect();
    return {
      width: score.width,
      offset: Math.abs(bar.x + bar.width / 2 - score.x - score.width / 2),
    };
  });
  assert(mobile.width <= 390);
  assert(mobile.offset < 1);
  await page
    .getByRole("button", { name: "Restart exercise", exact: true })
    .click();
  await until(
    async () =>
      (await state()).practice.running && (await state()).practice.index === 0,
  );
  await page.screenshot({
    path: "web/test-results/staff-practice-mobile.png",
    fullPage: true,
  });
  assert.deepEqual(errors, []);
  const saved = (await state()).practice.config;
  other.close();
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
  assert.deepEqual((await state()).practice.config, saved);
  assert.equal((await state()).practice.running, false);
  assert.equal((await state()).metronome.running, false);
  console.log(
    "PASS: native Wait/Scroll timing, exact held pitch answers, isolated reset/stop + global clicks, atomic invalid config, all Staff generators/visible options, Grand/88, local numeric drafts, native Cb spelling SVG, fullscreen, persistence and no browser errors",
  );
} finally {
  other?.close();
  await browser?.close();
  await stop();
  await rm(data, { recursive: true, force: true });
}
