import { it, expect } from "vitest";
import { staffClefs, frameX, type TimelineNote } from "./notationModel";
it("Grand joins treble and bass while single staff modes retain their clef", () => {
  expect(staffClefs("grand")).toEqual(["treble", "bass"]);
  expect(staffClefs("bass")).toEqual(["bass"]);
});
it("native sample positions map to stable notation geometry independent of viewport", () => {
  expect(frameX(48000, 48000, 100)).toBe(100);
  expect(frameX(96000, 48000, 100)).toBe(200);
});
it("enharmonic spelling stays separate from MIDI pitch in the reusable timeline", () => {
  const n: TimelineNote = {
    id: "flat",
    midiNote: 61,
    spelling: { letter: "d", accidental: "b", octave: 4 },
    startFrame: 0,
    endFrame: 48000,
  };
  expect(n.spelling.letter).toBe("d");
  expect(n.midiNote).toBe(61);
});
