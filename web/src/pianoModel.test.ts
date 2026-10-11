import { describe, expect, it } from "vitest";
import {
  pianoKeys,
  keyboardWidth,
  computerMap,
  VirtualInput,
} from "./pianoModel";
describe("global virtual piano ownership", () => {
  it("exposes all 88 piano keys with middle C=60 and monotonic geometry", () => {
    const keys = pianoKeys();
    expect(keys).toHaveLength(88);
    expect(keys[0]?.label).toBe("A0");
    expect(keys.at(-1)?.label).toBe("C8");
    expect(keys.find((k) => k.midi === 60)?.label).toBe("C4");
    expect(keys.filter((k) => !k.black)).toHaveLength(52);
  });
  it("keeps full keyboard geometry usable after a narrow resize", () => {
    expect(keyboardWidth(320)).toBe(1040);
    expect(keyboardWidth(1440)).toBe(1440);
  });
  it("reserves M and supports computer chords", () => {
    expect(computerMap.KeyM).toBeUndefined();
    expect([computerMap.KeyA, computerMap.KeyD, computerMap.KeyG]).toEqual([
      0, 4, 7,
    ]);
  });
  it("owns overlapping pointer and computer notes independently and ignores key repeats", () => {
    const sent: { type: string; params: Record<string, unknown> }[] = [];
    const input = new VirtualInput(
      (type, params) => sent.push({ type, params }),
      "browser-test",
    );
    input.on("pointer:1", 60, 90);
    input.on("key:KeyA", 60, 80);
    input.on("key:KeyA", 60, 80);
    input.off("pointer:1");
    expect(sent.map((s) => s.type)).toEqual([
      "virtual.noteOn",
      "virtual.noteOn",
      "virtual.noteOff",
    ]);
    expect(sent[0]?.params.instance).not.toBe(sent[1]?.params.instance);
    expect(sent[2]?.params.instance).toBe(sent[0]?.params.instance);
    input.release();
    input.release();
    expect(sent.map((s) => s.type)).toEqual([
      "virtual.noteOn",
      "virtual.noteOn",
      "virtual.noteOff",
      "virtual.disconnect",
    ]);
    expect(
      sent.every(
        (s) =>
          s.type.startsWith("virtual.") && s.params.source === "browser-test",
      ),
    ).toBe(true);
    input.on("pointer:2", 64, 100);
    expect(Number(sent.at(-1)?.params.instance)).toBeGreaterThan(
      Number(sent[1]?.params.instance),
    );
  });
});
