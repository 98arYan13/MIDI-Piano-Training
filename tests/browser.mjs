import { browserPath } from "./browser-path.mjs";
import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { mkdtemp, rm, mkdir } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { chromium } from "../web/node_modules/playwright/index.mjs";
const data = await mkdtemp(join(tmpdir(), "sightreading-browser-")),
  port = 18768,
  url = `http://127.0.0.1:${port}`;
const child = spawn(
  "build/native/native/sightreading.exe",
  ["--port", String(port), "--data-dir", data],
  { stdio: "pipe" },
);
let browser;
const errors = [];
try {
  for (let i = 0; i < 100; i++) {
    try {
      if ((await fetch(url + "/api/v1/status")).ok) break;
    } catch {}
    await new Promise((r) => setTimeout(r, 100));
  }
  browser = await chromium.launch({
    executablePath:
      browserPath(),
    headless: true,
  });
  const context = await browser.newContext({
    viewport: { width: 1440, height: 1000 },
  });
  await context.route("**/*", (route) =>
    route.request().url().startsWith(url) ? route.continue() : route.abort(),
  );
  const page = await context.newPage();
  page.on("pageerror", (e) => errors.push(e.message));
  await page.goto(url);
  await page
    .getByRole("button", { name: "Start metronome", exact: true })
    .waitFor();
  await page
    .getByRole("button", { name: "Start metronome", exact: true })
    .click();
  await page
    .getByRole("button", { name: "Stop metronome", exact: true })
    .waitFor();
  let state = await (await fetch(url + "/api/v1/status")).json();
  const frame = state.sampleFrame;
  for (const name of [
    "Ear Training",
    "Flash Cards",
    "Play Along",
    "Guide",
    "Staff",
  ]) {
    await page.getByRole("link", { name: new RegExp(name) }).click();
    await page
      .getByRole("heading", {
        name: name === "Guide" ? "Practice, with everything in reach." : name,
        exact: true,
      })
      .waitFor();
    assert.equal(
      (await (await fetch(url + "/api/v1/status")).json()).metronome.running,
      true,
    );
  }
  await page.keyboard.press("m");
  await page.getByRole("dialog", { name: "Metronome", exact: true }).waitFor();
  const tempoInput = page.getByLabel("Quarter-note BPM");
  await tempoInput.click();
  await tempoInput.press("Control+A");
  await tempoInput.press("Backspace");
  await tempoInput.pressSequentially("40");
  assert.equal(await tempoInput.inputValue(), "40");
  await tempoInput.press("Enter");
  await page.getByLabel("Quarter-note BPM").fill("132");
  await page.getByLabel(/^Beat unit/).selectOption("8");
  const beatsInput = page.getByLabel("Beats per bar", { exact: true });
  await beatsInput.click();
  await beatsInput.press("Control+A");
  await beatsInput.press("Backspace");
  await beatsInput.pressSequentially("6");
  assert.equal(await beatsInput.inputValue(), "6");
  await beatsInput.press("Enter");
  await page.getByLabel(/^Beat divisions/).selectOption("triplet");
  await page.getByLabel("Beat 2 accent", { exact: true }).selectOption("mute");
  await page
    .getByLabel("Beat 3 accent", { exact: true })
    .selectOption("strong");
  await page
    .getByText(/Count-in edits take effect on the next start/)
    .waitFor();
  await page
    .getByRole("button", { name: "Close settings", exact: true })
    .click();
  await page
    .getByRole("button", { name: "Audio and MIDI settings", exact: true })
    .click();
  await page
    .getByRole("dialog", { name: "Audio & MIDI", exact: true })
    .waitFor();
  await page.keyboard.press("m");
  assert.equal(
    await page.getByRole("dialog", { name: "Metronome", exact: true }).count(),
    1,
  );
  await page
    .getByRole("button", { name: "Close settings", exact: true })
    .click();
  await page
    .getByRole("button", { name: "Toggle fullscreen", exact: true })
    .click();
  await page.waitForFunction(() => !!document.fullscreenElement);
  await page
    .getByRole("button", { name: "Metronome settings", exact: true })
    .click();
  await page.getByRole("dialog", { name: "Metronome", exact: true }).waitFor();
  await page
    .getByRole("button", { name: "Close settings", exact: true })
    .click();
  await page
    .getByRole("button", { name: "Toggle fullscreen", exact: true })
    .click();
  await page.reload();
  await page
    .getByRole("button", { name: "Stop metronome", exact: true })
    .waitFor();
  state = await (await fetch(url + "/api/v1/status")).json();
  assert.equal(state.metronome.settings.tempo, 132);
  assert.equal(state.metronome.settings.meter.denominator, 8);
  assert.equal(state.metronome.settings.meter.numerator, 6);
  assert.equal(state.metronome.settings.accents[1], "mute");
  assert.equal(state.metronome.settings.accents[2], "strong");
  assert.ok(state.sampleFrame > frame);
  await mkdir(".tools/visual-proof", { recursive: true });
  await page.screenshot({
    path: ".tools/visual-proof/shell-desktop.png",
    fullPage: true,
  });
  await page.setViewportSize({ width: 390, height: 844 });
  await page
    .getByRole("button", { name: "Metronome settings", exact: true })
    .click();
  await page.getByRole("dialog", { name: "Metronome", exact: true }).waitFor();
  await page.screenshot({
    path: ".tools/visual-proof/shell-mobile.png",
    fullPage: true,
  });
  assert.equal(
    await page.evaluate(
      () => document.documentElement.scrollWidth > innerWidth,
    ),
    false,
  );
  await page.close();
  assert.equal(
    (await (await fetch(url + "/api/v1/status")).json()).metronome.running,
    true,
  );
  assert.deepEqual(errors, []);
  console.log(
    "PASS five tabs, native controls, fullscreen dialogs, settings, reload, mobile width, browser closure, offline resources",
  );
} finally {
  await browser?.close();
  if (child.exitCode === null) {
    child.kill();
    await new Promise((r) => child.on("exit", r));
  }
  await rm(data, { recursive: true, force: true });
}
