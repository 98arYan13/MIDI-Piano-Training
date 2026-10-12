import { expect, it } from "vitest";
import { staffScore } from "./staffScore";
import { buildScore, scoreFrame } from "./scoreModel";
import type { TimelineNote } from "./notationModel";

it("preserves native Staff spelling, quarter-note timing and bar accidentals", () => {
  const notes: TimelineNote[] = Array.from({ length: 8 }, (_, i) => ({
    id: String(i),
    midiNote: 59,
    spelling: { letter: "c", accidental: "b", octave: 4 },
    startFrame: i * 24000,
    endFrame: (i + 1) * 24000,
    duration: "q",
    staff: "treble",
  }));
  const { notes: display, song } = staffScore(notes, "Gb", 120, 48000);
  const score = buildScore(display, song, 0, song.durationTicks);
  expect(score).toHaveLength(2);
  expect(score[0].sharps).toBe(-6);
  const tokens = score.flatMap((m) =>
    m.voices.filter((v) => v.clef === "treble").flatMap((v) => v.tokens),
  );
  expect(tokens).toHaveLength(8);
  expect(
    tokens.every(
      (t) =>
        t.duration === "q" &&
        t.pitches[0].spelling.letter === "c" &&
        !t.pitches[0].accidental,
    ),
  ).toBe(true);
  expect(display[7].startTick).toBe(7 * 384);
  expect(scoreFrame(song, display[7].startTick!, 48000)).toBe(
    notes[7].startFrame,
  );
  expect(notes[0].startTick).toBeUndefined();
});

it("prints chromatic cancellation and repetition using native spelling", () => {
  const notes: TimelineNote[] = [61, 61, 60].map((midiNote, i) => ({
    id: String(i),
    midiNote,
    spelling: {
      letter: "c",
      accidental: midiNote === 61 ? "#" : "n",
      octave: 4,
    },
    startFrame: i * 24000,
    endFrame: (i + 1) * 24000,
    staff: "bass",
  }));
  const { notes: display, song } = staffScore(notes, "Chromatic", 120, 48000);
  const sounding = buildScore(display, song, 0, song.durationTicks)[0]
    .voices.find((v) => v.clef === "bass")!
    .tokens.filter((t) => t.pitches.length);
  expect(sounding.map((t) => t.pitches[0].accidental)).toEqual([
    "#",
    undefined,
    "n",
  ]);
});
