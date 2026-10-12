import { browserPath } from "./browser-path.mjs";
import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { mkdtemp, rm, mkdir } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { chromium } from "../web/node_modules/playwright/index.mjs";
import WebSocket from "../web/node_modules/ws/wrapper.mjs";
const data = await mkdtemp(join(tmpdir(), "sightreading-keyboard-")),
  port = 18769,
  url = `http://127.0.0.1:${port}`;
const child = spawn(
  "build/native/native/sightreading.exe",
  ["--port", String(port), "--data-dir", data],
  { stdio: "pipe" },
);
let serviceOutput = "";
child.stdout.on("data", (b) => (serviceOutput += b));
child.stderr.on("data", (b) => (serviceOutput += b));
let browser, other;
const errors = [];
const wait = (ms) => new Promise((r) => setTimeout(r, ms));
const state = async () => await (await fetch(url + "/api/v1/status")).json();
const until = async (predicate) => {
  for (let i = 0; i < 150; i++) {
    if (child.exitCode !== null)
      throw Error(`Service exited ${child.exitCode}: ${serviceOutput}`);
    if (await predicate()) return;
    await wait(100);
  }
  throw Error("Native condition timed out");
};
try {
  await until(async () => {
    try {
      return (await state()).pianoReady;
    } catch {
      return false;
    }
  });
  browser = await chromium.launch({
    executablePath: browserPath(),
    headless: true,
  });
  const page = await browser.newPage({
    viewport: { width: 1440, height: 1000 },
  });
  page.on("pageerror", (e) => errors.push(e.message));
  await page.goto(url);
  await page.locator('[data-midi="60"]:enabled').waitFor();
  assert.equal(await page.locator(".piano-key").count(), 88);
  for (const count of [25, 32, 37, 49, 61, 76, 88]) {
    await page
      .getByLabel("Keyboard size", { exact: true })
      .selectOption(String(count));
    assert.equal(await page.locator(".piano-key").count(), count);
    assert.equal(await page.locator('[data-midi="60"]').count(), 1);
  }
  assert.equal(typeof (await state()).overload, "number");
  assert.equal(typeof (await state()).voiceRefusals, "number");
  assert.equal(
    await page.locator('[data-midi="60"]').getAttribute("aria-label"),
    "C4, MIDI 60",
  );
  await page.getByLabel("Staff view", { exact: true }).selectOption("grand");
  await page.locator('.staff-clefs[data-clefs="treble bass"] svg').waitFor();
  assert.equal(
    await page.locator('.staff-clefs[data-clefs="treble bass"] svg').count(),
    1,
  );
  assert.equal(await page.locator(".piano-key").count(), 88);
  // Another connected owner's note stands in for an unrelated physical owner at the service boundary.
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
      id: "other",
      type: "virtual.noteOn",
      params: {
        source: "other-owner",
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
  await page.keyboard.down("a");
  await page.keyboard.down("d");
  await page.keyboard.down("g");
  await until(
    async () => (await state()).heldNotes.filter((n) => n.held).length === 4,
  );
  const owned = (await state()).heldNotes.filter(
    (n) => n.held && n.instance !== 999,
  );
  assert.equal(new Set(owned.map((n) => n.instance)).size, 3);
  await page.getByRole("link", { name: /Ear Training/ }).click();
  assert.equal((await state()).heldNotes.filter((n) => n.held).length, 4);
  await page.keyboard.up("a");
  await page.keyboard.up("d");
  await page.keyboard.up("g");
  await until(
    async () => (await state()).heldNotes.filter((n) => n.held).length === 1,
  );
  await page.getByRole("link", { name: /Staff/ }).click();
  const key = page.locator('[data-midi="64"]');
  const cdp = await page.context().newCDPSession(page);
  await key.scrollIntoViewIfNeeded();
  const box = await key.boundingBox();
  await page.evaluate(() => {
    window.testPointerIds = [];
    document.addEventListener("pointerdown", (e) =>
      window.testPointerIds.push(e.pointerId),
    );
  });
  await cdp.send("Input.dispatchTouchEvent", {
    type: "touchStart",
    touchPoints: [
      { x: box.x + 5, y: box.y + 110, id: 7 },
      { x: box.x + 12, y: box.y + 120, id: 8 },
    ],
  });
  await until(
    async () =>
      (await state()).heldNotes.filter((n) => n.held && n.note === 64)
        .length === 2,
  );
  const pointerId = await page.evaluate(() => window.testPointerIds[0]);
  await key.dispatchEvent("pointercancel", { pointerId, pointerType: "touch" });
  await until(
    async () =>
      (await state()).heldNotes.filter((n) => n.held && n.note === 64)
        .length === 1,
  );
  await page.getByRole("button", { name: "Sustain up", exact: true }).click();
  await page.evaluate(() => window.dispatchEvent(new Event("blur")));
  await until(async () => {
    const s = await state();
    return (
      s.heldNotes.filter((n) => n.held).length === 1 &&
      !s.heldNotes.some((n) => n.instance !== 999 && n.sustained)
    );
  });
  assert.equal(
    (await state()).heldNotes.find((n) => n.instance === 999)?.held,
    true,
  );
  await cdp.send("Input.dispatchTouchEvent", {
    type: "touchEnd",
    touchPoints: [],
  });
  await page.keyboard.press("m");
  await page.getByRole("dialog", { name: "Metronome", exact: true }).waitFor();
  await page.getByRole("button", { name: "Close settings" }).click();
  await page
    .getByRole("button", { name: "Focused range", exact: true })
    .click();
  assert.equal(await page.locator(".piano-key").count(), 37);
  await page.getByRole("button", { name: "Next octave", exact: true }).click();
  assert.equal(await page.locator('[data-midi="84"]').count(), 1);
  await page.getByLabel("Keyboard size", { exact: true }).selectOption("88");
  await page
    .getByRole("button", { name: "Hide keyboard", exact: true })
    .click();
  assert.equal(await page.locator(".piano-key").count(), 0);
  await page
    .getByRole("button", { name: "Show keyboard", exact: true })
    .click();
  assert.equal(await page.locator(".piano-key").count(), 88);
  await page.getByText("Keyboard settings", { exact: true }).click();
  await page.getByLabel("Virtual key velocity").fill("75");
  await page.getByLabel("Note labels", { exact: true }).uncheck();
  assert.equal(await page.locator(".piano-key span").count(), 0);
  await page.getByLabel("Note labels", { exact: true }).check();
  await page.getByText("Keyboard settings", { exact: true }).click();
  await page.setViewportSize({ width: 390, height: 844 });
  assert.equal(await page.locator(".piano-key").count(), 88);
  assert.equal(
    await page.evaluate(
      () => document.documentElement.scrollWidth <= innerWidth,
    ),
    true,
  );
  assert.equal(
    await page
      .locator(".piano-scroll")
      .evaluate((el) => el.scrollWidth > el.clientWidth),
    true,
  );
  await page
    .locator(".piano-scroll")
    .evaluate((el) => (el.scrollLeft = el.scrollWidth));
  assert.equal(await page.locator('[data-midi="108"]').isVisible(), true);
  await page.getByRole("button", { name: "Toggle fullscreen" }).click();
  assert.equal(await page.evaluate(() => !!document.fullscreenElement), true);
  await page.evaluate(() => document.exitFullscreen());
  await page
    .getByLabel("Practice mode", { exact: true })
    .selectOption("scroll");
  await page
    .getByRole("button", { name: "Start exercise", exact: true })
    .click();
  await wait(350);
  const first = await page
    .locator(".staff-track")
    .evaluate((el) => el.style.transform);
  await wait(350);
  assert.notEqual(
    await page.locator(".staff-track").evaluate((el) => el.style.transform),
    first,
  );
  await mkdir("web/test-results", { recursive: true });
  await page.screenshot({
    path: "web/test-results/keyboard-mobile.png",
    fullPage: true,
  });
  await page.setViewportSize({ width: 1440, height: 1000 });
  await page
    .getByRole("button", { name: "Stop exercise", exact: true })
    .click();
  await page.getByLabel("Staff view", { exact: true }).selectOption("grand");
  await page.screenshot({
    path: "web/test-results/keyboard-grand.png",
    fullPage: true,
  });
  assert.deepEqual(errors, []);
  console.log(
    "PASS: Grand/88/C4, native chord ownership, route persistence, multitouch cancellation, virtual pedal focus recovery, other owner untouched, M, responsive pan, fullscreen, native-clock scroll; no browser errors",
  );
} finally {
  other?.close();
  await browser?.close();
  if (child.exitCode === null) {
    const exited = new Promise((r) => child.once("exit", r));
    child.kill();
    await exited;
  }
  await rm(data, { recursive: true, force: true });
}
