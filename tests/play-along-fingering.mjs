import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { mkdtemp, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { chromium } from "../web/node_modules/playwright/index.mjs";
import { browserPath } from "./browser-path.mjs";
const data = await mkdtemp(join(tmpdir(), "sr-finger-ui-")),
  url = "http://127.0.0.1:18845";
let child;
function launch() {
  child = spawn(
    process.env.SR_FINGERING_EXECUTABLE ??
      "build/fingering-native/native/sightreading.exe",
    ["--port", "18845", "--data-dir", data, "--no-tray"],
    { stdio: "pipe", windowsHide: true },
  );
  child.stdout.on("data", (s) => (output += s));
  child.stderr.on("data", (s) => (output += s));
}
let browser,
  output = "",
  page;
launch();
const wait = (ms) => new Promise((r) => setTimeout(r, ms));
async function until(fn) {
  for (let i = 0; i < 300; i++) {
    if (child.exitCode !== null)
      throw Error("service exited " + child.exitCode + " " + output);
    if (await fn()) return;
    await wait(100);
  }
  await page?.screenshot({
    path: "web/test-results/fingering-failure.png",
    fullPage: true,
  });
  throw Error(
    "timeout " +
      output +
      " " +
      (await page?.locator(".fingering-controls").innerText()),
  );
}
async function api(path, method = "GET", body) {
  const r = await fetch(url + "/api/v1/" + path, {
    method,
    headers: {
      Origin: url,
      ...(body instanceof FormData
        ? {}
        : { "Content-Type": "application/json" }),
    },
    body:
      body === undefined
        ? undefined
        : body instanceof FormData
          ? body
          : JSON.stringify(body),
  });
  return { status: r.status, body: await r.json() };
}
let commandId = 0;
async function command(type, params = {}) {
  const r = await api("command", "POST", {
    version: 1,
    id: "finger-" + ++commandId,
    type,
    params,
  });
  assert.equal(r.body.ok, true, JSON.stringify(r));
}
async function stopped() {
  if (child.exitCode === null) {
    const exit = new Promise((r) => child.once("exit", r));
    child.kill();
    await exit;
  }
}
// Authored MIDI: repeated positions, an ascending thumb crossing, chord accidentals and a held inner voice.
function fixture() {
  const vlq = (n) => {
    const bytes = [n & 127];
    while ((n >>= 7)) bytes.unshift((n & 127) | 128);
    return bytes;
  };
  const tracks = [[], []];
  for (let hand = 0; hand < 2; hand++) {
    const events = [],
      base = hand ? 48 : 60,
      scale = [0, 0, 0, 0, 2, 4, 5, 7, 9, 11, 12, 12, 12, 12, 7, 0];
    scale.forEach((pitch, i) => {
      events.push(
        [i * 480, 0x90, base + pitch, 80],
        [i * 480 + 480, 0x80, base + pitch, 0],
      );
    });
    events.push([0, 0x90, base, 80], [480, 0x80, base, 0]);
    for (const pitch of [base + 1, base + 5, base + 8])
      events.push([7680, 0x90, pitch, 80], [9600, 0x80, pitch, 0]);
    events.push(
      [10080, 0x90, base, 80],
      [13920, 0x80, base, 0],
      [10560, 0x90, base + 7, 80],
      [11040, 0x80, base + 7, 0],
      [11520, 0x90, base + 9, 80],
      [12000, 0x80, base + 9, 0],
    );
    events.sort((a, b) => a[0] - b[0]);
    let tick = 0;
    for (const [time, ...event] of events) {
      tracks[hand].push(...vlq(time - tick), ...event);
      tick = time;
    }
    tracks[hand].push(0, 255, 47, 0);
  }
  const header = Buffer.from([
    77, 84, 104, 100, 0, 0, 0, 6, 0, 1, 0, 2, 1, 224,
  ]);
  return Buffer.concat([
    header,
    ...tracks.map((t) => {
      const h = Buffer.alloc(8);
      h.write("MTrk");
      h.writeUInt32BE(t.length, 4);
      return Buffer.concat([h, Buffer.from(t)]);
    }),
  ]);
}
try {
  await until(async () => {
    try {
      return (await fetch(url + "/api/v1/status")).ok;
    } catch {
      return false;
    }
  });
  browser = await chromium.launch({
    executablePath: browserPath(),
    headless: true,
  });
  page = await browser.newPage({ viewport: { width: 1430, height: 1039 } });
  const errors = [],
    commands = [];
  page.on("pageerror", (e) => errors.push(e.message));
  page.on("request", (r) => {
    if (r.url().endsWith("/command"))
      commands.push(JSON.parse(r.postData()).type);
  });
  await page.goto(url);
  await page.getByRole("link", { name: /Play Along/ }).click();
  const form = new FormData();
  form.append("file", new Blob([fixture()]), "Fingering QA.mid");
  const imported = await api("songs/import", "POST", form);
  assert.equal(imported.status, 201);
  await page.reload();
  await page.getByRole("button", { name: /Fingering QA.*sec/i }).click();
  await page
    .getByRole("button", { name: "Generate fingerings", exact: true })
    .waitFor({ timeout: 3000 });
  assert.equal(await page.getByLabel("Fingering tips").inputValue(), "medium");
  const id = (await api("status")).body.transport.songId;
  await command("transport.seek", { tick: 480 });
  await command("transport.pause");
  const before = (await api("status")).body.transport;
  const beforeEvents = (
    await api(`songs/${id}/events?fromTick=0&toTick=100000`)
  ).body.notes;
  commands.length = 0;
  let pollFailed = false;
  let recoveredPoll = false;
  await page.route(`**/api/v1/songs/${id}/fingering`, async (route) => {
    if (route.request().method() === "GET" && !pollFailed) {
      const response = await route.fetch();
      const body = await response.json();
      if (body.job.state === "running") {
        pollFailed = true;
        await route.fulfill({
          status: 503,
          contentType: "application/json",
          body: JSON.stringify({
            error: { message: "Temporary polling failure" },
          }),
        });
        return;
      }
      await route.fulfill({ response });
    } else if (pollFailed && !recoveredPoll && route.request().method() === "GET") {
      const response = await route.fetch();
      const body = await response.json();
      body.revision = 0; // Recover without a revision-triggered remount.
      body.job.state = "running";
      recoveredPoll = true;
      await route.fulfill({ response, body: JSON.stringify(body) });
    } else await route.continue();
  });
  await page
    .getByRole("button", { name: "Generate fingerings", exact: true })
    .click();
  await page
    .getByRole("button", { name: "Cancel fingering generation" })
    .waitFor();
  await page.route(`**/api/v1/songs/${id}/fingering/cancel`, (route) =>
    route.fulfill({
      status: 409,
      contentType: "application/json",
      body: JSON.stringify({
        error: { message: "Cancellation already decided" },
      }),
    }),
  );
  await page
    .getByRole("button", { name: "Cancel fingering generation" })
    .click();
  await page
    .getByRole("alert")
    .filter({ hasText: "Cancellation already decided" })
    .waitFor();
  await page.unroute(`**/api/v1/songs/${id}/fingering/cancel`);
  await until(
    async () =>
      (await api(`songs/${id}/fingering`)).body.job.state === "completed",
  );
  await page
    .getByRole("button", { name: "Regenerate fingerings", exact: true })
    .waitFor();
  assert(pollFailed);
  assert(recoveredPoll);
  await until(async () => (await page.getByRole("alert").count()) === 0);
  await page.unroute(`**/api/v1/songs/${id}/fingering`);
  const generated = (await api("songs/" + id)).body;
  generated.notes = (
    await api(`songs/${id}/events?fromTick=0&toTick=100000`)
  ).body.notes;
  const musical = (notes) => notes.map(({ fingering, ...note }) => note);
  assert.deepEqual(
    musical(generated.notes),
    musical(beforeEvents),
    "Generation preserves exact native musical event identities/timing/spelling/ownership",
  );
  assert.equal(generated.fingeringRun.model.name, "ArGNN");
  assert(generated.fingeringRun.assigned > 0);
  assert(generated.notes.some((n) => n.fingering?.hand === "left"));
  assert(generated.notes.some((n) => n.fingering?.hand === "right"));
  const afterGeneration = (await api("status")).body.transport;
  assert.equal((await api("status")).body.transport.songTick, before.songTick);
  assert.equal((await api("status")).body.transport.state, before.state);
  const count = () => page.locator(".vf-score-finger").count();
  await page.getByLabel("Fingering tips").selectOption("full");
  await until(
    async () => (await api("status")).body.preferences.fingeringTips === "full",
  );
  await wait(600);
  await until(async () => (await count()) > 0);
  const full = await count();
  await page.screenshot({
    path: "web/test-results/fingering-full.png",
    fullPage: true,
  });
  await page.getByLabel("Fingering tips").selectOption("medium");
  await until(
    async () =>
      (await api("status")).body.preferences.fingeringTips === "medium",
  );
  await wait(600);
  const medium = await count();
  await page.getByLabel("Fingering tips").selectOption("pro");
  await until(
    async () => (await api("status")).body.preferences.fingeringTips === "pro",
  );
  await wait(600);
  const pro = await count();
  assert(full > medium && medium > pro, JSON.stringify({ full, medium, pro }));
  console.log("density", { full, medium, pro });
  await page.getByLabel("Fingering tips").selectOption("full");
  await page
    .getByRole("button", { name: "Edit fingering", exact: true })
    .click();
  assert.equal(
    await page
      .getByRole("button", { name: "Finger 3", exact: true })
      .isEnabled(),
    false,
  );
  const note = page.locator(".fingering-edit-note").first();
  await note.focus();
  await page.keyboard.press("Enter");
  const selected = JSON.parse(await note.getAttribute("data-note-ids"));
  assert.equal(selected.length, 2, "Merged unison retains both native IDs");
  await page.getByRole("button", { name: "Finger 3", exact: true }).click();
  await until(async () => {
    const s = (await api(`songs/${id}/events?fromTick=0&toTick=100000`)).body;
    return selected.every(
      (id) =>
        s.notes.find((n) => String(n.id) === id)?.fingering?.source ===
        "manual",
    );
  });
  await page
    .getByRole("button", { name: "Edit fingering", exact: true })
    .click();
  await page.locator(".fingering-edit-note").first().focus();
  await page.keyboard.press("Enter");
  await page
    .getByRole("button", { name: "Use automatic", exact: true })
    .click();
  await until(async () => {
    const s = (await api(`songs/${id}/events?fromTick=0&toTick=100000`)).body;
    return selected.every(
      (id) =>
        s.notes.find((n) => String(n.id) === id)?.fingering?.source ===
        "generated",
    );
  });
  await wait(600);
  assert(!commands.includes("transport.load"), JSON.stringify(commands));
  console.log("manual/reset passed");
  const oldDigits = await page
    .locator(".vf-score-finger")
    .evaluateAll((es) => es.map((e) => e.textContent));
  await page
    .getByRole("button", { name: "Regenerate fingerings", exact: true })
    .click();
  await page
    .getByRole("button", { name: "Cancel fingering generation" })
    .waitFor();
  assert.deepEqual(
    await page
      .locator(".vf-score-finger")
      .evaluateAll((es) => es.map((e) => e.textContent)),
    oldDigits,
  );
  await page
    .getByRole("button", { name: "Cancel fingering generation" })
    .click();
  await until(
    async () =>
      (await api(`songs/${id}/fingering`)).body.job.state !== "running",
  );
  console.log("cancel passed");
  await page
    .getByRole("button", { name: "Regenerate fingerings", exact: true })
    .click();
  await page
    .getByRole("button", { name: "Cancel fingering generation" })
    .waitFor();
  await page.getByRole("button", { name: /BWV 846.*sec/ }).click();
  await until(async () => (await api("status")).body.transport.songId !== id);
  await until(
    async () =>
      (await api(`songs/${id}/fingering`)).body.job.state !== "running",
  );
  await wait(600);
  assert.equal(await count(), 0);
  console.log("song switch passed");
  await page
    .getByRole("button", { name: "Generate fingerings", exact: true })
    .click();
  const chordId = (await api("status")).body.transport.songId;
  await until(
    async () =>
      (await api(`songs/${chordId}/fingering`)).body.job.state === "completed",
  );
  console.log("chord complete", chordId);
  await page.getByLabel("Fingering tips").selectOption("full");
  await until(async () => (await count()) > 0);
  await page.screenshot({
    path: "web/test-results/fingering-polyphony.png",
    fullPage: true,
  });
  const boxes = await page.locator(".vf-score-finger").evaluateAll((es) =>
    es.map((e) => {
      const t = e.querySelector("text"),
        style = getComputedStyle(t),
        ctx = document.createElement("canvas").getContext("2d");
      ctx.font = `${style.fontSize} ${style.fontFamily}`;
      const m = ctx.measureText(t.textContent),
        p = t.getStartPositionOfChar(0),
        matrix = t.getScreenCTM(),
        baseline = new DOMPoint(p.x, p.y).matrixTransform(matrix);
      return {
        x: baseline.x - m.actualBoundingBoxLeft * matrix.a,
        y: baseline.y - m.actualBoundingBoxAscent * matrix.d,
        w: (m.actualBoundingBoxLeft + m.actualBoundingBoxRight) * matrix.a,
        h: (m.actualBoundingBoxAscent + m.actualBoundingBoxDescent) * matrix.d,
        text: t.textContent,
      };
    }),
  );
  for (let i = 0; i < boxes.length; i++)
    for (let j = i + 1; j < boxes.length; j++) {
      const a = boxes[i],
        b = boxes[j];
      assert(
        !(
          Math.min(a.x + a.w, b.x + b.w) > Math.max(a.x, b.x) + 1 &&
          Math.min(a.y + a.h, b.y + b.h) > Math.max(a.y, b.y) + 1
        ),
        "Finger label collision " + JSON.stringify([a, b]),
      );
    }
  const musicBoxes = await page
    .locator(
      ".vf-notehead,.vf-beam,.vf-stavetie,.vf-accidental,.score-measure svg > text",
    )
    .evaluateAll((es) =>
      es.flatMap((e) =>
        (e.tagName === "text"
          ? [e]
          : [...e.querySelectorAll("text,path,polygon")]
        )
          .filter((t) => !t.closest(".vf-score-finger"))
          .map((t) => {
            if (t.tagName === "text") {
              const style = getComputedStyle(t),
                ctx = document.createElement("canvas").getContext("2d");
              ctx.font = `${style.fontSize} ${style.fontFamily}`;
              const m = ctx.measureText(t.textContent),
                p = t.getStartPositionOfChar(0),
                matrix = t.getScreenCTM(),
                b = new DOMPoint(p.x, p.y).matrixTransform(matrix);
              return {
                x: b.x - m.actualBoundingBoxLeft * matrix.a,
                y: b.y - m.actualBoundingBoxAscent * matrix.d,
                w:
                  (m.actualBoundingBoxLeft + m.actualBoundingBoxRight) *
                  matrix.a,
                h:
                  (m.actualBoundingBoxAscent + m.actualBoundingBoxDescent) *
                  matrix.d,
                kind: e.getAttribute("class"),
              };
            }
            const r = t.getBoundingClientRect();
            return {
              x: r.x,
              y: r.y,
              w: r.width,
              h: r.height,
              kind: e.getAttribute("class"),
            };
          }),
      ),
    );
  for (const a of boxes)
    for (const b of musicBoxes)
      assert(
        !(
          Math.min(a.x + a.w, b.x + b.w) > Math.max(a.x, b.x) + 1 &&
          Math.min(a.y + a.h, b.y + b.h) > Math.max(a.y, b.y) + 1
        ),
        "Finger/music collision " + JSON.stringify([a, b]),
      );
  await writeFile(
    "web/test-results/fingering-geometry.json",
    JSON.stringify(
      {
        density: { full, medium, pro },
        fingerInk: boxes,
        musicalInk: musicBoxes,
        musicalEventsUnchanged: true,
        before: { tick: before.songTick, state: before.state },
        after: { tick: afterGeneration.songTick, state: afterGeneration.state },
      },
      null,
      2,
    ),
  );
  await page.getByLabel("Hide passage controls").click();
  await until(() => page.getByLabel("Hide passage controls").isChecked());
  await page.screenshot({
    path: "web/test-results/fingering-hidden-passage.png",
    fullPage: true,
  });
  await page.getByRole("button", { name: "Focus mode", exact: true }).click();
  await page.screenshot({
    path: "web/test-results/fingering-focus.png",
    fullPage: true,
  });
  await page.setViewportSize({ width: 390, height: 844 });
  await page.screenshot({
    path: "web/test-results/fingering-narrow.png",
    fullPage: true,
  });
  await command("transport.configure", { loop: true, a: 0, b: 1920 });
  await command("transport.seek", { tick: 960 });
  assert.equal((await api("status")).body.transport.config.loop, true);
  await page.keyboard.press("Escape");
  await page.setViewportSize({ width: 1430, height: 1039 });
  await page.getByRole("button", { name: /Fingering QA.*sec/ }).click();
  await command("transport.seek", { tick: 8000 });
  await wait(600);
  await page.screenshot({
    path: "web/test-results/fingering-accidental-chord.png",
    fullPage: true,
  });
  await command("transport.seek", { tick: 10560 });
  await wait(600);
  await page.screenshot({
    path: "web/test-results/fingering-accidental-held.png",
    fullPage: true,
  });
  const held = generated.notes.find(
    (n) => n.startTick === 10080 && n.midiNote === 60,
  );
  assert(held);
  assert.equal(
    await page
      .locator(`.vf-score-finger[data-note-ids='["${held.id}"]']`)
      .count(),
    1,
    "Tied held note has only one attack digit",
  );
  const stalePath = "**/api/v1/songs/starter-five-finger/fingering";
  let delayed = false;
  await page.route(stalePath, async (route) => {
    if (route.request().method() === "GET" && !delayed) {
      delayed = true;
      const response = await route.fetch();
      await wait(1800);
      await route.fulfill({ response });
    } else await route.continue();
  });
  await page.getByRole("button", { name: /Five Finger Patterns.*sec/ }).click();
  await page
    .getByRole("button", { name: "Generate fingerings", exact: true })
    .click();
  await until(
    async () =>
      (await api("songs/starter-five-finger/fingering")).body.job.state ===
      "completed",
  );
  await page
    .getByRole("button", { name: "Regenerate fingerings", exact: true })
    .waitFor({ timeout: 5000 });
  await until(async () => (await count()) > 0);
  await page.unroute(stalePath);
  await page.route(
    "**/api/v1/songs/starter-five-finger/fingering/generate",
    (route) =>
      route.fulfill({
        status: 503,
        contentType: "application/json",
        body: JSON.stringify({
          error: {
            code: "fingering_setup_missing",
            message:
              "Run scripts/setup-fingering.ps1 to install the local fingering runtime and weights",
          },
        }),
      }),
  );
  await page
    .getByRole("button", { name: "Regenerate fingerings", exact: true })
    .click();
  await page
    .getByRole("alert")
    .filter({ hasText: "setup-fingering.ps1" })
    .waitFor();
  await page.unroute("**/api/v1/songs/starter-five-finger/fingering/generate");
  // Zero is a valid summary count, including a generated run with no eligible notes.
  await page.route(stalePath, async route => {
    const response = await route.fetch();
    const body = await response.json();
    body.summary = { ...body.summary, assigned: 0, unresolved: 0, unsupported: 0 };
    await route.fulfill({ response, body: JSON.stringify(body) });
  });
  await page.getByRole("button", { name: /Fingering QA.*sec/i }).click();
  await page.getByRole("button", { name: /Five Finger Patterns.*sec/ }).click();
  await page.getByText("0 assigned · 0 unresolved · 0 unsupported", { exact: true }).waitFor();
  await page.unroute(stalePath);
  await stopped();
  launch();
  await until(async () => {
    try {
      return (await api("status")).body.pianoReady;
    } catch {
      return false;
    }
  });
  assert.equal((await api("status")).body.preferences.fingeringTips, "full");
  assert((await api("songs/" + id)).body.fingeringRevision >= 3);
  assert.deepEqual(errors, []);
  console.log(
    "PASS real ArGNN UI both hands, density " +
      JSON.stringify({ full, medium, pro }) +
      ", edit/reset, cancel, song switch, no transport load, focus/narrow, seek/loop, restart; zero browser errors",
  );
} finally {
  await browser?.close();
  await stopped();
  assert(resolve(data).startsWith(resolve(tmpdir()) + "\\"));
  await rm(data, { recursive: true, force: true });
}
