import { expect, it } from "vitest";
import { buildScore, scoreFrame, scoreTick, type ScoreMetadata } from "./scoreModel";
import { interpolateScore } from "./EngravedScore";
const song: ScoreMetadata = {
  ppq: 384,
  durationTicks: 2304,
  tempoMap: [{ tick: 0, microseconds: 800000, seconds: 0 }],
  meterMap: [{ tick: 0, numerator: 3, denominator: 8 }],
  keyMap: [],
  tracks: [
    { id: 1, title: "Piano RH", staff: "split" },
    { id: 2, title: "Piano LH", staff: "split" },
  ],
};
it("maps selection marker frames back to ticks across tempo changes", () => {
  const metadata = { ...song, tempoMap: [
    { tick: 0, microseconds: 500000 }, { tick: 768, microseconds: 1000000 },
  ] };
  for (const tick of [0, 384, 768, 960, 2304])
    expect(scoreTick(metadata, scoreFrame(metadata, tick, 48000), 48000)).toBe(tick);
  expect(scoreTick(metadata, -48000, 48000)).toBe(0);
  expect(scoreTick(metadata, 1e9, 48000)).toBe(metadata.durationTicks);
});
const note = (pitch: number, start: number, end: number, track = 1) => ({
  id: `${pitch}:${start}:${track}`,
  midiNote: pitch,
  startTick: start,
  endTick: end,
  trackId: track,
  spelling: { letter: "c" as const, octave: 4 },
  startFrame: 0,
  endFrame: 0,
});
it("keeps a continuous 3/4 melody above a held inner voice without false pauses", () => {
  const metadata = { ...song, ppq: 256, durationTicks: 2304,
    meterMap: [{ tick: 0, numerator: 3, denominator: 4 }] };
  const score = buildScore([
    note(77, 0, 512), note(65, 512, 640), note(60, 512, 768),
    note(65, 640, 768), note(77, 768, 1280),
    note(74, 1280, 1408), note(62, 1280, 1536), note(70, 1408, 1536),
  ], metadata, 0, 1536);
  for (const bar of score) {
    const melody = bar.voices.find(v => v.clef === "treble")!;
    expect(melody.tokens.every(t => t.pitches.length)).toBe(true);
    const inner = bar.voices.filter(v => v.clef === "treble")[1];
    expect(inner.tokens.filter(t => !t.pitches.length).every(t => t.hiddenRest)).toBe(true);
    const held = inner.tokens.find(t => t.pitches.length)!;
    expect(held.duration).toBe("q");
  }
  const pause = buildScore([note(77, 0, 512)], metadata, 0, 768)[0];
  expect(pause.voices.find(v => v.clef === "treble")!.tokens.at(-1)!.hiddenRest).toBe(false);
});
it("keeps a named practice melody on treble when its arpeggio dips below middle C", () => {
  const metadata = {
    ...song,
    tracks: [{ id: 1, title: "Practice melody", staff: "split" }],
  };
  const low = { ...note(59, 0, 192), staff: "bass" as const };
  const score = buildScore([low, note(62, 192, 384)], metadata, 0, 576);
  expect(
    score[0].voices.find((v) => v.clef === "treble")!.tokens[0].pitches[0]
      ?.midi,
  ).toBe(59);
  expect(
    score[0].voices
      .find((v) => v.clef === "bass")!
      .tokens.every((t) => !t.pitches.length),
  ).toBe(true);
  const override = {
    ...metadata,
    tracks: [{ id: 1, title: "Practice melody", staff: "bass" }],
  };
  expect(
    buildScore([low], override, 0, 576)[0].voices.find(
      (v) => v.clef === "bass",
    )!.tokens[0].pitches[0]?.midi,
  ).toBe(59);
});
it("keeps piano backing chords together on bass, including their upper tones", () => {
  const metadata = {
    ...song,
    tracks: [{ id: 2, title: "Piano backing", staff: "split" }],
  };
  const score = buildScore(
    [55, 59, 62, 65].map((p) => note(p, 0, 576, 2)),
    metadata,
    0,
    576,
  );
  expect(
    score[0].voices.find((v) => v.clef === "treble")!.tokens[0].pitches,
  ).toEqual([]);
  expect(
    score[0].voices
      .find((v) => v.clef === "bass")!
      .tokens[0].pitches.map((p) => p.midi),
  ).toEqual([55, 59, 62, 65]);
  const override = {
    ...metadata,
    tracks: [{ id: 2, title: "Piano backing", staff: "treble" }],
  };
  expect(
    buildScore([note(65, 0, 576, 2)], override, 0, 576)[0].voices.find(
      (v) => v.clef === "treble",
    )!.tokens[0].pitches,
  ).toHaveLength(1);
});
it("orders upper melody before a sustained lower voice at the reported bar boundary", () => {
  const score = buildScore(
    [
      note(62, 3936, 4224),
      note(72, 4032, 4128),
      note(71, 4128, 4224),
      note(69, 4224, 4416),
      note(60, 4224, 4416),
    ],
    { ...song, durationTicks: 4608 },
    4032,
    4608,
  );
  const voices = score[0].voices.filter((v) => v.clef === "treble");
  expect(voices).toHaveLength(2);
  expect(voices[0].tokens[0].pitches.map((p) => p.midi)).toEqual([72]);
  expect(voices[1].tokens[0].pitches.map((p) => p.midi)).toEqual([62]);
  expect(voices[1].tokens[0].tiePrevious).toBe(true);
});
it("engraves performed duplicate notes once and restores sixteenth rhythm in 3/8", () => {
  const score = buildScore(
    [note(76, 16, 110), note(76, 18, 112, 3), note(75, 114, 204)],
    song,
    0,
    1152,
  );
  expect(score[0].numerator).toBe(3);
  expect(score[0].denominator).toBe(8);
  const sounding = score[0].voices.flatMap((v) =>
    v.tokens.filter((t) => t.pitches.length),
  );
  expect(sounding).toHaveLength(2);
  expect(sounding.map((t) => t.duration)).toEqual(["16", "16"]);
  expect(sounding.map((t) => t.startTick)).toEqual([0, 96]);
});
it("keeps named left-hand arpeggios on bass even above middle C", () => {
  const score = buildScore([note(64, 0, 192, 2)], song, 0, 576);
  expect(
    score[0].voices.find((v) => v.tokens.some((t) => t.pitches.length))?.clef,
  ).toBe("bass");
});
it("prints accidentals only when pitch changes and resets at the barline", () => {
  const score = buildScore(
    [
      note(75, 0, 96),
      note(75, 96, 192),
      note(74, 192, 288),
      note(75, 576, 672),
    ],
    song,
    0,
    1152,
  );
  const signs = score.flatMap((m) =>
    m.voices.flatMap((v) =>
      v.tokens.flatMap((t) => t.pitches.map((p) => p.accidental)),
    ),
  );
  expect(signs).toEqual(["#", undefined, "n", "#"]);
});
it("fills rests, preserves dotted rhythm and ties a sustained pitch over a barline", () => {
  const score = buildScore([note(69, 192, 864)], song, 0, 1152);
  const first = score[0].voices.find((v) => v.clef === "treble")!.tokens;
  expect(first[0].pitches).toEqual([]);
  expect(first[0].duration).toBe("8");
  expect(first.at(-1)?.tieNext).toBe(true);
  expect(
    score[1].voices.find((v) => v.clef === "treble")!.tokens[0].tiePrevious,
  ).toBe(true);
  const dotted = buildScore([note(69, 0, 288)], song, 0, 576)[0].voices[0]
    .tokens[0];
  expect([dotted.duration, dotted.dots]).toEqual(["8", 1]);
});
it("native tempo changes determine score frames independently of display quantization", () => {
  expect(
    scoreFrame(
      {
        ...song,
        tempoMap: [
          { tick: 0, microseconds: 500000, seconds: 0 },
          { tick: 384, microseconds: 1000000, seconds: 0.5 },
        ],
      },
      768,
      48000,
    ),
  ).toBe(72000);
});
it("maps native frames to unequal engraving intervals and inverts dragging", () => {
  const points = [
    { frame: 0, x: 20 },
    { frame: 100, x: 60 },
    { frame: 400, x: 180 },
  ];
  expect(interpolateScore(points, 250)).toBe(120);
  expect(interpolateScore(points, 120, true)).toBe(250);
  expect(interpolateScore(points, 240, true)).toBe(550);
  expect(interpolateScore(points, -20, true)).toBe(-100);
});
it("uses key signatures for flat spelling and prints the cancelling natural", () => {
  const flat = { ...song, keyMap: [{ tick: 0, sharps: -1 }] };
  const pitches = buildScore(
    [note(70, 0, 192), note(71, 192, 384)],
    flat,
    0,
    576,
  )[0].voices[0].tokens.flatMap((t) => t.pitches);
  expect(
    pitches.map((p) => [
      p.spelling.letter,
      p.spelling.accidental,
      p.accidental,
    ]),
  ).toEqual([
    ["b", "b", undefined],
    ["b", "n", "n"],
  ]);
});

it("preserves chord pitch identities and merges agreeing or conflicting unisons without changing music", () => {
  const f = (finger: 1 | 2 | 3) => ({ finger, hand: "right" as const, source: "generated" as const, tips: ["phraseStart" as const] });
  const plain = [note(67, 0, 768), note(60, 0, 768), { ...note(60, 0, 768), id: "duplicate" }];
  const annotated = plain.map((n, i) => ({ ...n, fingering: f(i === 0 ? 3 : 1) }));
  const result = buildScore(annotated, song, 0, 1152);
  const pitches = result[0].voices[0].tokens[0].pitches;
  expect(pitches.map(p => [p.midi, p.noteIds, p.fingering?.finger])).toEqual([[60, [plain[1].id, "duplicate"], 1], [67, [plain[0].id], 3]]);
  const strip = (score: ReturnType<typeof buildScore>) => score.map(m => ({ ...m, voices: m.voices.map(v => ({ ...v, tokens: v.tokens.map(t => ({ ...t, pitches: t.pitches.map(({ noteIds, fingering, ...p }) => p) })) })) }));
  expect(strip(result)).toEqual(strip(buildScore(plain, song, 0, 1152)));
  annotated[2].fingering = f(2);
  expect(buildScore(annotated, song, 0, 1152)[0].voices[0].tokens[0].pitches[0].fingering?.finger).toBeNull();
  expect(buildScore(annotated, song, 576, 1152)[0].voices[0].tokens.find(t => t.pitches.length)?.tiePrevious).toBe(true);
});

it("keeps old, muted and named-hand notation identical with annotations across tied windows", () => {
  const notes = [ { ...note(55, 0, 960), highlight: "muted" as const }, note(65, 0, 960, 2) ];
  const decorated = notes.map(n => ({ ...n, fingering: { finger: 2 as const, hand: n.trackId === 1 ? "right" as const : "left" as const, source: "generated" as const, tips: [] } }));
  const plain = buildScore(notes, song, 0, 1152), annotated = buildScore(decorated, song, 0, 1152);
  for (let i = 0; i < plain.length; i++) for (let v = 0; v < plain[i].voices.length; v++) {
    const oldVoice = plain[i].voices[v], newVoice = annotated[i].voices[v];
    expect(newVoice.clef).toBe(oldVoice.clef);
    expect(newVoice.tokens.map(t => ({ ...t, pitches: t.pitches.map(({ fingering, ...p }) => p) }))).toEqual(oldVoice.tokens);
  }
  expect(annotated[0].voices[0].tokens[0].muted).toBe(true);
  const continuation = buildScore(decorated, song, 576, 1152)[0].voices[0].tokens[0];
  expect(continuation.tiePrevious).toBe(true);
  expect(continuation.pitches[0].noteIds).toEqual([notes[0].id]);
});
