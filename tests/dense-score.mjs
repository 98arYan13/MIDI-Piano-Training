import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { mkdtemp, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { chromium } from "../web/node_modules/playwright/index.mjs";
import { browserPath } from "./browser-path.mjs";
const data = await mkdtemp(join(tmpdir(), "sr-dense-score-")),
  url = "http://127.0.0.1:18787";
const service = spawn(
  "build/native/native/sightreading.exe",
  ["--port", "18787", "--data-dir", data, "--no-tray"],
  { stdio: "pipe" },
);
let browser,
  id = 0;
const pause = (ms) => new Promise((r) => setTimeout(r, ms));
const state = async () => await (await fetch(url + "/api/v1/status")).json();
const command = async (type, params = {}) => {
  const r = await (
    await fetch(url + "/api/v1/command", {
      method: "POST",
      headers: { Origin: url, "Content-Type": "application/json" },
      body: JSON.stringify({ version: 1, id: ++id, type, params }),
    })
  ).json();
  assert(r.ok, JSON.stringify(r));
};
const until = async (fn) => {
  for (let i = 0; i < 150; i++) {
    try {
      if (await fn()) return;
    } catch {}
    await pause(100);
  }
  throw Error("Dense score timeout");
};
const variable = (n) => {
  const bytes = [n & 127];
  while ((n >>= 7)) bytes.unshift((n & 127) | 128);
  return bytes;
};
const chunk = (name, bytes) => {
  const length = Buffer.alloc(4);
  length.writeUInt32BE(bytes.length);
  return Buffer.concat([Buffer.from(name), length, Buffer.from(bytes)]);
};
function pianoTrack(left, channel) {
  const title = Buffer.from(left ? "Piano LH" : "Piano RH");
  const bytes = [
    0,
    255,
    3,
    title.length,
    ...title,
    0,
    255,
    88,
    4,
    3,
    3,
    24,
    8,
    0,
    255,
    81,
    3,
    12,
    53,
    0,
  ];
  const melody = left
    ? [45, 52, 57, 52, 45, 52, 57, 52, 45, 52, 57, 52]
    : [76, 75, 76, 75, 76, 71, 74, 72, 69, 64, 69, 71];
  for (let bar = 0; bar < 8; bar++)
    for (let i = 0; i < 12; i++)
      bytes.push(
        ...variable(bar === 0 && i === 0 ? 0 : 2),
        144 + channel,
        melody[i],
        80,
        ...variable(46),
        128 + channel,
        melody[i],
        0,
      );
  bytes.push(0, 255, 47, 0);
  return chunk("MTrk", bytes);
}
try {
  await until(async () => Boolean((await state()).pianoReady));
  await command("audio.settings", { outputMode: "silent" });
  const header = Buffer.from([0, 1, 0, 4, 1, 128]);
  const midi = Buffer.concat([
    chunk("MThd", header),
    pianoTrack(false, 0),
    pianoTrack(true, 1),
    pianoTrack(false, 2),
    pianoTrack(true, 3),
  ]);
  const form = new FormData();
  form.append("file", new Blob([midi]), "Dense duplicated piano.mid");
  const response = await fetch(url + "/api/v1/songs/import", {
    method: "POST",
    headers: { Origin: url },
    body: form,
  });
  assert.equal(response.status, 201);
  const song = await response.json();
  assert.equal(
    song.notes.length,
    384,
    "native retains all duplicated performance events",
  );
  await command("transport.load", { id: song.id });
  await command("transport.configure", { practiceEnabled: false });
  browser = await chromium.launch({
    executablePath: browserPath(),
    headless: true,
  });
  const page = await browser.newPage({
    viewport: { width: 1265, height: 1039 },
  });
  const errors = [];
  page.on("pageerror", (e) => errors.push(e.message));
  await page.goto(url + "/#play");
  const first = page.locator('.score-measure[data-measure="1"]');
  await first.locator(".vf-notehead").first().waitFor();
  assert.equal(
    await first.locator(".vf-notehead").count(),
    24,
    "two printed hands, twelve notes each, no copied-track duplicates",
  );
  assert(
    (await first.locator(".vf-beam").count()) >= 2,
    "both hands use meter-based beams",
  );
  const upper = await first
    .locator(
      '.vf-stavenote[data-start-tick="0"] .vf-notehead > text:first-child',
    )
    .evaluateAll((es) =>
      es.map((e) => ({
        x: Number(e.getAttribute("x")),
        y: Number(e.getAttribute("y")),
      })),
    );
  assert.equal(upper.length, 2);
  assert.equal(upper[0].x, upper[1].x, "hands align at the same native beat");
  const positions = await first
    .locator(".vf-notehead > text:first-child")
    .evaluateAll((es) =>
      es.map((e) => ({
        x: Number(e.getAttribute("x")),
        y: Number(e.getAttribute("y")),
      })),
    );
  const xs = [...new Set(positions.map((p) => p.x))].sort((a, b) => a - b);
  assert.equal(xs.length, 12);
  for (let i = 1; i < xs.length; i++)
    assert(
      xs[i] - xs[i - 1] > 20,
      "dense successive notes retain readable clearance",
    );
  const glyphs = await first.locator("text").allTextContents();
  assert.equal(
    glyphs.filter((t) => t === "\uE261").length,
    1,
    "only the D-sharp cancellation needs a natural",
  );
  await command("transport.seek", { tick: 7 * 576 });
  await until(
    async () =>
      (await page.locator('.score-measure[data-measure="8"]').count()) === 1,
  );
  const score = page.getByRole("img", { name: /Song practice notation/ });
  await score.scrollIntoViewIfNeeded();
  await page.screenshot({ path: "web/test-results/dense-score.png" });
  assert.deepEqual(errors, []);
  console.log(
    "PASS dense 3/8 duplicated piano: original events retained, 24 unique printed notes/bar, beam groups, accidental memory, aligned hands, readable spacing, late seek",
  );
} finally {
  await browser?.close();
  if (service.exitCode === null) {
    const stopped = new Promise((r) => service.once("exit", r));
    await fetch(url + "/api/v1/shutdown", {
      method: "POST",
      headers: { Origin: url },
    }).catch(() => service.kill());
    await stopped;
  }
  await rm(data, { recursive: true, force: true });
}
