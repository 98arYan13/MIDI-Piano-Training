import { it, expect } from "vitest";
import { staffClefs, frameX, spelledLabel, segmentFingerprint, type TimelineNote } from "./notationModel";
it("cached segment identity ignores other windows but includes spelling and result", () => {
  const n: TimelineNote={id:'one',midiNote:60,spelling:{letter:'c',octave:4},startFrame:0,endFrame:48000};
  const key=segmentFingerprint([n],0,48000,110,'grand');
  expect(segmentFingerprint([n,{...n,id:'later',startFrame:1000000,endFrame:1000500}],0,48000,110,'grand')).toBe(key);
  expect(segmentFingerprint([{...n,highlight:'hit'}],0,48000,110,'grand')).not.toBe(key);
  expect(segmentFingerprint([{...n,spelling:{letter:'b',accidental:'#',octave:3}}],0,48000,110,'grand')).not.toBe(key);
});
it("native Cb and double accidental labels retain written pitch", () => {
  expect(spelledLabel({letter:'c',accidental:'b',octave:4})).toBe('Cb4');
  expect(spelledLabel({letter:'f',accidental:'##',octave:5})).toBe('F##5');
});
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
