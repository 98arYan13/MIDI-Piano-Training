import { browserPath } from "./browser-path.mjs";
import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { mkdtemp, rm, readFile, mkdir, copyFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { chromium } from "../web/node_modules/playwright/index.mjs";
const data = await mkdtemp(join(tmpdir(), "sr-song-")),
  port = 18773,
  url = `http://127.0.0.1:${port}`;
let child,
  browser,
  output = "",
  id = 0;
const wait = (ms) => new Promise((r) => setTimeout(r, ms));
const state = async () => await (await fetch(url + "/api/v1/status")).json();
const until = async (fn) => {
  for (let i = 0; i < 150; i++) {
    if (child?.exitCode !== null && child?.exitCode !== undefined)
      throw Error(output);
    if (await fn()) return;
    await wait(100);
  }
  throw Error("Song condition timeout");
};
const command = async (type, params = {}) => {
  const r = await (
    await fetch(url + "/api/v1/command", {
      method: "POST",
      headers: { Origin: url, "Content-Type": "application/json" },
      body: JSON.stringify({ version: 1, id: `song-${++id}`, type, params }),
    })
  ).json();
  assert.equal(r.ok, true, JSON.stringify(r));
  return r.result;
};
const api = async (path, options = {}) => {
  const r = await fetch(url + "/api/v1/songs" + path, {
    ...options,
    headers: { Origin: url, ...options.headers },
  });
  return { status: r.status, body: await r.json() };
};
const stop = async () => {
  if (child?.exitCode === null) {
    const p = new Promise((r) => child.once("exit", r));
    child.kill();
    await p;
  }
};
const launch = () => {
  child = spawn(
    "build/native/native/sightreading.exe",
    ["--port", String(port), "--data-dir", data],
    { stdio: "pipe" },
  );
  child.stdout.on("data", (s) => (output += s));
  child.stderr.on("data", (s) => (output += s));
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
  const initial = (await api("")).body;
  assert.equal(initial.length, 5);
  assert.equal((await state()).transport.songId, "");
  const original = await readFile("assets/music/twinkle.mid");
  const form = new FormData();
  form.append("file", new Blob([original]), "Own.mid");
  const imported = await api("/import", { method: "POST", body: form });
  assert.equal(imported.status, 201);
  const song = imported.body;
  assert.equal(song.tracks.length, 2);
  assert.ok(song.notes.length > 14);
  assert.equal(song.tempoMap.length, 1);
  assert.ok(song.durationSeconds > 1);
  const preserved = Buffer.from(
    await (
      await fetch(url + `/api/v1/songs/${song.id}/original`)
    ).arrayBuffer(),
  );
  assert.deepEqual(preserved, original);
  const bad = new FormData();
  bad.append("file", new Blob(["MThd"]), "broken.mid");
  assert.equal(
    (await api("/import", { method: "POST", body: bad })).status,
    400,
  );
  assert.equal((await api("")).body.length, 6);
  const edited = await api(`/${song.id}`, {
    method: "PATCH",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      title: "Round trip",
      tracks: [{ id: 0, practice: true, backing: false, staff: "grand" }],
    }),
  });
  assert.equal(edited.body.title, "Round trip");
  const events = await api(`/${song.id}/events?fromTick=0&toTick=1920&limit=3`);
  assert.equal(events.body.notes.length, 3);
  assert.ok(events.body.notes[0].endFrame > 0);
  assert.ok(events.body.notes[0].spelling);
  assert.equal((await api(`/${song.id}/events?fromTick=-1`)).status, 400);
  await command("virtual.noteOn", {
    source: "song-live",
    channel: 0,
    note: 76,
    velocity: 90,
    instance: 777,
  });
  await command("metronome.start");
  await until(async () => {
    const s = await state();
    return s.heldNotes.some((n) => n.instance === 777) && s.metronome.running;
  });
  await command("transport.load", { id: song.id });
  await command("transport.play");
  await wait(200);
  assert.equal((await state()).transport.state, "playing");
  for (const style of ["Off", "Root", "Triad", "Root+5", "Arp", "Bossa Nova"])
    await command("transport.configure", { accompaniment: style });
  await command("transport.configure", {
    a: 0,
    b: 1920,
    loop: true,
    speed: 2,
    hearPractice: true,
    mode: "continuous",
  });
  await command("transport.seek", { tick: 960 });
  await wait(200);
  await command("transport.pause");
  assert.equal((await state()).transport.state, "paused");
  await command("transport.restart");
  await command("transport.stop");
  await until(async () => {
    const s = await state();
    return (
      s.metronome.running &&
      s.heldNotes.some((n) => n.instance === 777) &&
      !s.heldNotes.some((n) => n.bus === "playback")
    );
  });
  browser = await chromium.launch({
    executablePath:
      browserPath(),
    headless: true,
  });
  const page = await browser.newPage({
    viewport: { width: 1500, height: 1100 },
  });
  const errors = [];
  page.on("pageerror", (e) => errors.push(e.message));
  await page.goto(url);
  await page.getByRole("link", { name: /Play Along/ }).click();
  await page.getByText("Your MIDI library").waitFor();
  await page.getByRole("button", { name: /Twinkle Twinkle.*sec/ }).click();
  await page.getByRole("button", { name: "Play", exact: true }).waitFor();
  await page.getByRole("button", { name: "Play", exact: true }).click();
  await until(async () => (await state()).transport.state === "playing");
  await page.getByRole("button", { name: "Pause", exact: true }).click();
  await until(async () => (await state()).transport.state === "paused");
  await page.getByText("Playback settings, loops & tracks", {exact:true}).click();
  await page.getByLabel("Auto accompaniment").selectOption("Arp");
  await until(
    async () => (await state()).transport.config.accompaniment === "Arp",
  );
  await page.getByRole("button", { name: "Restart", exact: true }).click();
  await page.getByRole("button", { name: "Stop", exact: true }).click();
  await page.getByRole("img", { name: /Song practice notation/ }).waitFor();
  await page
    .getByLabel("Upload MIDI")
    .setInputFiles({
      name: "browser.mid",
      mimeType: "audio/midi",
      buffer: original,
    });
  await until(async () => (await api("")).body.length === 7);
  await page.setViewportSize({ width: 390, height: 844 });
  await page.screenshot({
    path: "web/test-results/play-along-mobile.png",
    fullPage: true,
  });
  await page.setViewportSize({ width: 1500, height: 1100 });
  await page.screenshot({
    path: "web/test-results/play-along.png",
    fullPage: true,
  });
  assert.deepEqual(errors, []);
  await browser.close();
  browser = null;
  await api(`/${song.id}`, { method: "DELETE" });
  assert.ok((await api("/trash")).body.some((s) => s.id === song.id));
  await api(`/${song.id}/restore`, { method: "POST" });
  await stop();
  launch();
  await until(async () => {
    try {
      return (await state()).pianoReady;
    } catch {
      return false;
    }
  });
  assert.equal((await api(`/${song.id}`)).body.title, "Round trip");
  assert.equal((await state()).transport.state, "stopped");
  console.log(
    "PASS actual binary multipart/import/original/window/title/assignment/trash/restore/restart + native playback/loop/accompaniment/live/metronome + installed Edge UI/upload/mobile; no browser errors",
  );
} finally {
  if (browser) await browser.close();
  await stop();
  const target = resolve(data);
  if (!target.startsWith(resolve(tmpdir()) + "\\"))
    throw Error("Unsafe test cleanup path");
  await rm(target, { recursive: true, force: true });
}
