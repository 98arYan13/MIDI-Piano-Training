import { it, expect } from "vitest";
import { Stave } from "vexflow/bravura";
import { notationNote } from "./notationNote";
import type { TimelineNote } from "./notationModel";
it("native C-flat and B-sharp keep their diatonic glyph position and accidental", () => {
  const note = (midiNote: number, spelling: TimelineNote["spelling"]) =>
    notationNote(
      [{ id: "native", midiNote, spelling, startFrame: 0, endFrame: 48000 }],
      "treble",
    ).setStave(new Stave(0, 35, 480));
  const cb = note(59, { letter: "c", accidental: "b", octave: 4 }),
    b = note(59, { letter: "b", accidental: "n", octave: 3 });
  expect(cb.getYs()[0]).toBe(b.getYs()[0] - 5);
  expect(cb.getModifiers()[0].getCategory()).toBe("Accidental");
  expect((cb.getModifiers()[0] as unknown as { type: string }).type).toBe("b");
  const bs = note(60, { letter: "b", accidental: "#", octave: 3 }),
    c = note(60, { letter: "c", accidental: "n", octave: 4 });
  expect(bs.getYs()[0]).toBe(c.getYs()[0] + 5);
  expect((bs.getModifiers()[0] as unknown as { type: string }).type).toBe("#");
});
