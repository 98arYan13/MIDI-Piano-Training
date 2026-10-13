import { describe, it, expect } from "vitest";
import { acceptSnapshot, estimateFrame, returnPosition } from "./clock";
import type { SnapshotMessage } from "./types";
const snap = (generation: number, frame: number) =>
  ({
    version: 1,
    type: "snapshot",
    generation,
    state: {
      generation,
      sampleFrame: frame,
      sampleRate: 48000,
      audioReady: true,
    },
  }) as SnapshotMessage;
describe("native visual clock", () => {
  it("returns immediately with smooth native-clock progress over two seconds", () => {
    const animation = { from: 10, to: 2, startFrame: 48000, durationFrames: 96000 };
    expect(returnPosition(animation, 48000)).toBe(10);
    expect(returnPosition(animation, 72000)).toBeLessThan(10);
    expect(returnPosition(animation, 96000)).toBe(6);
    expect(returnPosition(animation, 144000)).toBe(2);
    expect(returnPosition(animation, 200000)).toBe(2);
  });
  it("accepts first state and rejects obsolete generation/frame", () => {
    expect(acceptSnapshot(null, snap(2, 100))).toBe(true);
    expect(acceptSnapshot(snap(2, 100), snap(1, 200))).toBe(false);
    expect(acceptSnapshot(snap(2, 100), snap(2, 99))).toBe(false);
    expect(acceptSnapshot(snap(2, 100), snap(3, 0))).toBe(true);
  });
  it("interpolates at native sample rate only while audio is ready", () => {
    expect(estimateFrame(snap(1, 48000), 100, 150)).toBe(50400);
    const stopped = snap(1, 100);
    stopped.state.audioReady = false;
    expect(estimateFrame(stopped, 0, 50)).toBe(100);
  });
});
import { clockOffset } from "./clock";
it("calibrates native clock against the midpoint of a ping round trip", () => {
  expect(clockOffset(100, 140, 2e9)).toBe(1880);
});
