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
  // Original synthetic polyphony matching the reported held lower note / upper beam.
  const events = [
    [64, 0, 96],
    [69, 96, 192],
    [71, 192, 480],
    [62, 480, 768],
    [72, 576, 672],
    [71, 672, 768],
    [69, 768, 960],
    [60, 768, 960],
  ]
    .flatMap(([pitch, start, end]) => [
      [start, 144, pitch, 80],
      [end, 128, pitch, 0],
    ])
    .sort((a, b) => a[0] - b[0] || a[1] - b[1]);
  const name = Buffer.from("Piano RH");
  const polyBytes = [
    0,
    255,
    3,
    name.length,
    ...name,
    0,
    255,
    88,
    4,
    3,
    3,
    24,
    8,
  ];
  let previousTick = 0;
  for (const [tick, status, pitch, velocity] of events) {
    polyBytes.push(...variable(tick - previousTick), status, pitch, velocity);
    previousTick = tick;
  }
  polyBytes.push(...variable(1152 - previousTick), 255, 47, 0);
  const polyForm = new FormData();
  polyForm.append(
    "file",
    new Blob([
      Buffer.concat([
        chunk("MThd", Buffer.from([0, 1, 0, 1, 1, 128])),
        chunk("MTrk", polyBytes),
      ]),
    ]),
    "Held lower voice.mid",
  );
  const polySong = await (
    await fetch(url + "/api/v1/songs/import", {
      method: "POST",
      headers: { Origin: url },
      body: polyForm,
    })
  ).json();
  await command("transport.load", { id: polySong.id });
  await until(
    async () =>
      (await page.getByLabel("Song title").inputValue()) === "Held lower voice",
  );
  await command("transport.seek", { tick: 576 });
  await until(
    async () =>
      (await page
        .locator(
          '.score-measure[data-measure="2"] .vf-stavenote[data-start-tick="576"] .vf-notehead text',
        )
        .evaluateAll(
          (es) => es.filter((e) => Number(e.getAttribute("y")) < 170).length,
        )) === 2,
  );
  const polyGeometry = await page
    .locator(
      '.score-measure[data-measure="2"] .vf-stavenote[data-start-tick="576"]',
    )
    .evaluateAll((es) =>
      es
        .filter((e) => e.querySelector(".vf-notehead"))
        .map((e) => ({
          head: Number(e.querySelector(".vf-notehead text").getAttribute("y")),
          stem: e.querySelector(".vf-stem")?.getBBox().y ?? null,
        })),
    );
  const higher = polyGeometry.find((n) => n.head < 100),
    lower = polyGeometry.find((n) => n.head > 100 && n.head < 170);
  assert(
    higher.stem === null,
    "upper melody uses a beam rather than an individual flag",
  );
  const beams = await page
    .locator('.score-measure[data-measure="2"] .vf-beam')
    .evaluateAll((es) =>
      es.map((e) => ({
        y: e.getBBox().y,
        bottom: e.getBBox().y + e.getBBox().height,
      })),
    );
  assert(
    beams.some((b) => b.y < higher.head - 20 && b.bottom <= higher.head + 5),
    "upper melody beam and stems stay above its noteheads",
  );
  assert(
    beams.some((b) => b.bottom > lower.head + 20),
    "held lower voice beam and stems descend away from the melody",
  );
  const ties = await page
    .locator('.score-measure[data-measure="2"] .vf-stavetie')
    .evaluateAll((es) =>
      es.map((e) => ({
        y: e.getBBox().y,
        bottom: e.getBBox().y + e.getBBox().height,
      })),
    );
  assert(
    ties.some((t) => t.y >= lower.head - 6 && t.bottom > lower.head),
    "lower-voice tie curves below the note, away from the upper melody",
  );
  await page.screenshot({ path: "web/test-results/polyphonic-score.png" });
  // Every MIDI key signature must fit in the fixed section on both staves.
  for (const fifths of [-7, -3, 3, 7]) {
    const form = new FormData();
    form.append("file", new Blob([Buffer.concat([
      chunk("MThd", Buffer.from([0, 0, 0, 1, 1, 128])),
      chunk("MTrk", [0, 255, 89, 2, fifths & 255, 0,
        0, 144, 64, 80, ...variable(384), 128, 64, 0, 0, 255, 47, 0]),
    ])]), `Signature ${fifths}.mid`);
    const imported = await (await fetch(url + "/api/v1/songs/import", {
      method: "POST", headers: { Origin: url }, body: form,
    })).json();
    await command("transport.load", { id: imported.id });
    const key = new Map([[-7, "Cb"], [-3, "Eb"], [3, "A"], [7, "C#"]]).get(fifths);
    await until(async () => await page.locator(`.score-gutter[data-key="${key}"]`).count() === 1);
    const geometry = await page.locator('.score-gutter').evaluate(el => {
      const svg = el.querySelector('svg');
      return { width: svg.getBoundingClientRect().width,
        edge: Math.max(...[...svg.querySelectorAll('text')].map(e => e.getBBox().x + e.getBBox().width)),
        glyphs: svg.querySelectorAll('.vf-keysignature text').length,
        background: getComputedStyle(el).backgroundImage };
    });
    assert.equal(geometry.glyphs, Math.abs(fifths) * 2, 'complete signature on each staff');
    assert(geometry.edge < geometry.width, 'all signature glyphs fit inside fixed section');
    assert(geometry.background.includes('linear-gradient') && geometry.background.includes('28px'), 'passing score fades at fixed edge');
  }
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
