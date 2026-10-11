import { request } from "node:http";
import WebSocket from "../web/node_modules/ws/wrapper.mjs";
import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { mkdtemp, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
const port = 18765,
  base = `http://127.0.0.1:${port}`,
  data = await mkdtemp(join(tmpdir(), "sightreading-api-"));
let child;
async function start() {
  child = spawn(
    "build/native/native/sightreading.exe",
    ["--port", String(port), "--data-dir", data],
    { stdio: "pipe" },
  );
  child.on("error", () => {});
  for (let i = 0; i < 150; i++) {
    try {
      if ((await fetch(base + "/api/v1/status")).ok) return;
    } catch {}
    await new Promise((r) => setTimeout(r, 100));
  }
  throw Error("Native service did not start");
}
async function stop() {
  child.kill();
  await new Promise((r) => child.on("exit", r));
}
async function command(type, params = {}) {
  return (
    await fetch(base + "/api/v1/command", {
      method: "POST",
      headers: { "Content-Type": "application/json", Origin: base },
      body: JSON.stringify({ version: 1, id: 42, type, params }),
    })
  ).json();
}
async function socket() {
  const ws = new WebSocket(base.replace("http", "ws") + "/api/v1/events", {
    headers: { Origin: base },
  });
  return await new Promise((resolve, reject) => {
    ws.onmessage = (e) => {
      const m = JSON.parse(e.data);
      if (m.type === "snapshot") resolve({ ws, m });
    };
    ws.onerror = reject;
  });
}
try {
  await start();
  assert.equal(
    (
      await fetch(base + "/api/v1/status", {
        headers: { Origin: "https://evil.example" },
      })
    ).status,
    403,
  );
  assert.equal(
    await new Promise((resolve, reject) => {
      const req = request(
        base + "/api/v1/status",
        { headers: { Host: "evil.example" } },
        (res) => {
          res.resume();
          resolve(res.statusCode);
        },
      );
      req.on("error", reject);
      req.end();
    }),
    403,
  );
  assert.equal(
    (
      await fetch(base + "/api/v1/command", {
        method: "POST",
        headers: { Origin: base },
        body: "{",
      })
    ).status,
    400,
  );
  assert.equal((await command("not-a-command")).error.code, "unknown_command");
  for (const headers of [{ Origin: "https://evil.example" }, {}]) {
    assert.equal(
      await new Promise((resolve, reject) => {
        const ws = new WebSocket(
          base.replace("http", "ws") + "/api/v1/events",
          { headers },
        );
        ws.on("unexpected-response", (_, res) => {
          res.resume();
          resolve(res.statusCode);
        });
        ws.on("open", () => {
          ws.close();
          reject(Error("Unexpected hostile WS upgrade"));
        });
        ws.on("error", () => {});
      }),
      403,
    );
  }
  assert.equal(
    (await fetch(base + "/api/v1/command", { method: "POST", body: "{}" }))
      .status,
    403,
  );
  const ping = await command("clock.ping");
  assert.equal(ping.ok, true);
  assert.ok(ping.result.engineTimeNs > 0);
  assert.ok(ping.result.generation > 0);
  assert.equal((await command("audio.settings", { volume: 0.37 })).ok, true);
  assert.equal(
    (await command("metronome.config", { tempo: 121, countIn: 0 })).ok,
    true,
  );
  await command("metronome.start");
  await new Promise((r) => setTimeout(r, 100));
  const a = await socket();
  a.ws.close();
  const b = await socket();
  assert.equal(b.m.state.metronome.running, true);
  assert.ok(b.m.state.sampleFrame >= a.m.state.sampleFrame);
  b.ws.close();
  let before = await (await fetch(base + "/api/v1/status")).json();
  await command("audio.settings", { outputMode: "silent" });
  await command("audio.settings", { monitoring: false });
  await command("audio.settings", { monitoring: true });
  if (before.midiOutputs.length) {
    await command("audio.settings", {
      externalDevice: before.midiOutputs[0].id,
    });
    const opened = (await (await fetch(base + "/api/v1/status")).json())
      .externalPortOpens;
    await command("audio.settings", { volume: 0.37 });
    await command("audio.settings", { velocityCurve: 1.1 });
    assert.equal(
      (await (await fetch(base + "/api/v1/status")).json()).externalPortOpens,
      opened,
    );
    assert.ok(opened > 0);
  }
  await new Promise((r) => setTimeout(r, 100));
  let after = await (await fetch(base + "/api/v1/status")).json();
  assert.equal(after.audioReady, true);
  assert.ok(after.sampleFrame > before.sampleFrame);
  assert.equal(after.metronome.running, true);
  const c = await socket(),
    d = await socket();
  function wsCommand(ws, id, type, params) {
    return new Promise((resolve, reject) => {
      const timeout = setTimeout(() => reject(Error("reply timeout")), 3000);
      const listener = (e) => {
        const reply = JSON.parse(e.data);
        if (reply.id === id) {
          clearTimeout(timeout);
          ws.removeEventListener("message", listener);
          resolve(reply);
        }
      };
      ws.addEventListener("message", listener);
      ws.send(JSON.stringify({ version: 1, id, type, params }));
    });
  }
  assert.equal(
    (
      await wsCommand(c.ws, "c", "virtual.noteOn", {
        source: "keyboard",
        note: 60,
        velocity: 100,
        instance: 1,
      })
    ).ok,
    true,
  );
  assert.equal(
    (
      await wsCommand(d.ws, "d", "virtual.noteOn", {
        source: "keyboard",
        note: 60,
        velocity: 100,
        instance: 1,
      })
    ).ok,
    true,
  );
  await new Promise((r) => setTimeout(r, 100));
  let notes = (
    await (await fetch(base + "/api/v1/status")).json()
  ).heldNotes.filter((n) => n.held);
  assert.equal(notes.length, 2);
  assert.notEqual(notes[0].source, notes[1].source);
  c.ws.close();
  await new Promise((r) => setTimeout(r, 100));
  notes = (
    await (await fetch(base + "/api/v1/status")).json()
  ).heldNotes.filter((n) => n.held);
  assert.equal(notes.length, 1);
  d.ws.close();
  // Apply now is a live reset, never a global Panic or routine WASAPI rebuild.
  const active = await socket();
  await wsCommand(active.ws, "live", "virtual.noteOn", {
    source: "keys",
    note: 65,
    velocity: 100,
    instance: 8,
  });
  await new Promise((r) => setTimeout(r, 50));
  const beforeApply = await (await fetch(base + "/api/v1/status")).json();
  assert.equal(
    (await command("audio.settings", { monitoring: false })).result.deferred,
    true,
  );
  assert.equal(
    (await command("audio.applyPending", { applyNow: true })).result.applied,
    true,
  );
  await new Promise((r) => setTimeout(r, 100));
  const afterApply = await (await fetch(base + "/api/v1/status")).json();
  assert.equal(afterApply.audioReady, true);
  assert.ok(afterApply.sampleFrame > beforeApply.sampleFrame);
  assert.equal(afterApply.metronome.running, true);
  assert.equal(
    afterApply.heldNotes.filter((n) => n.held && n.bus === "live").length,
    0,
  );
  active.ws.close();
  await command("audio.settings", { monitoring: true });
  const identities = new Set();
  for (let i = 0; i < 130; i++) {
    const { ws } = await socket();
    const reply = await wsCommand(ws, `reload-${i}`, "virtual.noteOn", {
      source: `fresh-${i}`,
      note: 60,
      velocity: 100,
      instance: 1,
    });
    assert.equal(reply.ok, true);
    identities.add(reply.result.sourceId);
    await new Promise((resolve) => {
      ws.on("close", resolve);
      ws.close();
    });
  }
  assert.ok(identities.size <= 32);
  await new Promise((r) => setTimeout(r, 100));
  assert.equal(
    (await (await fetch(base + "/api/v1/status")).json()).heldNotes.filter(
      (n) => n.held,
    ).length,
    0,
  );
  console.log(
    `PASS 130 reconnects reused ${identities.size} native source IDs without leaked notes`,
  );
  await Promise.all([
    command("metronome.meter", { denominator: 8 }),
    command("metronome.meter", { numerator: 6 }),
  ]);
  await Promise.all([
    command("metronome.accent", { beat: 1, accent: "mute" }),
    command("metronome.accent", { beat: 2, accent: "strong" }),
  ]);
  await Promise.all([
    command("audio.channel", { channel: 0, enabled: false }),
    command("audio.channel", { channel: 1, enabled: false }),
  ]);
  await stop();
  await start();
  const state = await (await fetch(base + "/api/v1/status")).json();
  assert.equal(state.settings.volume, 0.37);
  assert.equal(state.settings.channels.includes(0), false);
  assert.equal(state.settings.channels.includes(1), false);
  assert.equal(state.metronome.settings.tempo, 121);
  assert.equal(state.metronome.running, false);
  assert.equal(state.metronome.settings.meter.denominator, 8);
  assert.equal(state.metronome.settings.meter.numerator, 6);
  assert.equal(state.metronome.settings.accents[1], "mute");
  assert.equal(state.metronome.settings.accents[2], "strong");
  console.log(
    "PASS strict origins, JSON, commands, persistence, WebSocket reconnect continuity",
  );
} finally {
  if (child?.exitCode === null) await stop();
  await rm(data, { recursive: true, force: true });
}
