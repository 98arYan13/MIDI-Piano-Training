import { it, expect } from "vitest";
import { ModifierContext, Stave, TickContext } from "vexflow/bravura";
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

it("formats close chord accidentals into separate columns before timeline placement", () => {
  const chord = notationNote(
    ["c", "e", "g"].map((letter, i) => ({
      id: `chord-${i}`, midiNote: [60, 64, 67][i],
      spelling: { letter: letter as "c" | "e" | "g", accidental: "n", octave: 4 },
      startFrame: 0, endFrame: 48000,
    })), "treble",
  ).setStave(new Stave(0, 35, 480));
  new TickContext().addTickable(chord).preFormat().setX(100);
  const shifts = chord.getModifiers().map(modifier => modifier.getXShift());
  // Naturals a third apart are too tall to share the same column.
  expect(shifts[0]).not.toBe(shifts[1]);
  expect(shifts[1]).not.toBe(shifts[2]);
});

it("uses a downward stem for notes above the treble middle line", () => {
  const note = notationNote([{ id: "high", midiNote: 84,
    spelling: { letter: "c", octave: 6 }, startFrame: 0, endFrame: 48000,
  }], "treble");
  expect(note.getStemDirection()).toBe(-1);
});

it("separates simultaneous melody and whole-note accompaniment at the same pitch", () => {
  const stave = new Stave(0, 35, 480), modifiers = new ModifierContext();
  const melody = notationNote([{ id: "melody", midiNote: 62,
    spelling: { letter: "d", octave: 4 }, duration: "q", startFrame: 0, endFrame: 32000,
  }], "treble", modifiers).setStave(stave);
  const backing = notationNote([{ id: "backing", midiNote: 62,
    spelling: { letter: "d", octave: 4 }, duration: "w", startFrame: 0, endFrame: 128000,
  }], "treble", modifiers).setStave(stave);
  new TickContext().addTickable(melody).addTickable(backing).preFormat().setX(100);
  expect(melody.getXShift()).not.toBe(backing.getXShift());
});
