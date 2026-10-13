import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { mkdtemp, rm, readFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
const data = await mkdtemp(join(tmpdir(), "sr-fingering-")),
  port = 18789,
  base = `http://127.0.0.1:${port}`;
let child,
  output = "";
const musicalTransport = ({ engineTimeNs, sampleFrame, ...transport }) =>
  transport;
const wait = (ms) => new Promise((r) => setTimeout(r, ms));
async function api(path, method = "GET", body) {
  const r = await fetch(base + "/api/v1/" + path, {
    method,
    headers: {
      Origin: base,
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
async function until(fn) {
  for (let i = 0; i < 200; i++) {
    if (await fn()) return;
    await wait(100);
  }
  throw Error("Timeout " + output.slice(-2000));
}
async function start() {
  child = spawn(
    process.env.SR_FINGERING_EXECUTABLE ||
      "build/native/native/sightreading.exe",
    ["--port", String(port), "--data-dir", data, "--no-tray"],
    { stdio: "pipe", windowsHide: true },
  );
  child.stdout.on("data", (b) => (output = (output + b).slice(-8000)));
  child.stderr.on("data", (b) => (output = (output + b).slice(-8000)));
  await until(async () => {
    try {
      const status = await api("status");
      if (status.status !== 200) throw Error(JSON.stringify(status));
      return status.body.pianoReady;
    } catch {
      return false;
    }
  });
}
async function stop() {
  if (child?.exitCode === null) {
    const exit = new Promise((r) => child.once("exit", r));
    child.kill();
    await exit;
  }
}
async function command(type, params = {}) {
  const r = await api("command", "POST", {
    version: 1,
    id: "finger-test",
    type,
    params,
  });
  assert.equal(r.body.ok, true, JSON.stringify(r));
}
async function generate(id) {
  const r = await api(`songs/${id}/fingering/generate`, "POST", {});
  assert.equal(r.status, 202, JSON.stringify(r));
  return r.body;
}
async function terminal(id) {
  let result;
  await until(async () => {
    result = (await api(`songs/${id}/fingering`)).body;
    return result.job.state !== "running";
  });
  return result;
}
try {
  await start();
  const form = new FormData();
  form.append(
    "file",
    new Blob([await readFile("assets/music/twinkle.mid")]),
    "Authored.mid",
  );
  const imported = await api("songs/import", "POST", form);
  assert.equal(imported.status, 201);
  const song = imported.body,
    id = song.id;
  await command("transport.load", { id });
  await command("transport.seek", { tick: 480 });
  await command("transport.pause");
  await command("virtual.noteOn", {
    source: "fingering-live",
    channel: 0,
    note: 76,
    velocity: 80,
    instance: 913,
  });
  await until(async () =>
    (await api("status")).body.heldNotes.some((n) => n.instance === 913),
  );
  const before = (await api("status")).body;
  const initial = await generate(id);
  assert.equal(
    (await api(`songs/${id}/fingering/generate`, "POST", {})).status,
    409,
  );
  const completed = await terminal(id);
  assert.equal(completed.job.state, "completed", JSON.stringify(completed));
  assert.equal(completed.revision, 1);
  assert.ok(completed.summary.assigned > 0);
  assert.equal(completed.summary.model.name, "ArGNN");
  const after = (await api("status")).body;
  assert.deepEqual(
    musicalTransport(after.transport),
    musicalTransport(before.transport),
  );
  assert.ok(
    after.heldNotes.some((n) => n.instance === 913 && n.bus === "live"),
  );
  assert.equal(
    (
      await api(`songs/${id}/fingering/cancel`, "POST", {
        jobId: initial.jobId,
      })
    ).status,
    409,
  );
  const noteId = song.notes[0].id;
  assert.equal(
    (
      await api(`songs/${id}/fingering`, "PATCH", {
        noteIds: [String(noteId)],
        finger: 2,
      })
    ).status,
    400,
  );
  const next = await generate(id);
  assert.equal(
    (await api(`songs/${id}/fingering/cancel`, "POST", { jobId: next.jobId }))
      .status,
    200,
  );
  assert.equal((await terminal(id)).job.state, "cancelled");
  assert.equal((await api(`songs/${id}/fingering`)).body.revision, 1);
  await generate(id);
  const edit = await api(`songs/${id}/fingering`, "PATCH", {
    noteIds: [noteId],
    finger: 3,
  });
  assert.equal(edit.status, 200);
  assert.equal(edit.body.revision, 2);
  assert.equal((await terminal(id)).job.state, "failed");
  assert.equal((await api(`songs/${id}/fingering`)).body.revision, 2);
  await generate(id);
  await api(`songs/${id}`, "DELETE");
  assert.equal((await terminal(id)).job.state, "failed");
  await api(`songs/${id}/restore`, "POST", {});
  assert.equal((await api(`songs/${id}/fingering`)).body.revision, 2);
  await generate(id);
  await command("transport.load", { id: "starter-twinkle" });
  const switched = (await api("status")).body.transport;
  assert.equal((await terminal(id)).job.state, "completed");
  assert.deepEqual(
    musicalTransport((await api("status")).body.transport),
    musicalTransport(switched),
  );
  assert.equal((await api(`songs/${id}/fingering`)).body.revision, 3);
  await stop();
  await start();
  assert.equal((await api(`songs/${id}/fingering`)).body.revision, 3);
  assert.equal((await api(`songs/${id}/fingering`)).body.job.state, "idle");
  console.log(
    "PASS real ArGNN assigned; numeric edits; busy/cancel/stale/trash/original song; transport/live ownership; restart",
  );
} finally {
  await stop();
  await rm(data, { recursive: true, force: true });
}
