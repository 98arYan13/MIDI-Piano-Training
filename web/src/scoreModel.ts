import type { TimelineNote, SpelledPitch } from "./notationModel";
export type ScoreMetadata = {
  preserveSpelling?: boolean;
  ppq: number;
  durationTicks: number;
  tempoMap: { tick: number; microseconds: number; seconds?: number }[];
  meterMap: { tick: number; numerator: number; denominator: number }[];
  keyMap: { tick: number; sharps: number; minor?: boolean }[];
  tracks: { id: number; title: string; staff: string }[];
};
export type ScorePitch = {
  midi: number;
  highlight?: TimelineNote["highlight"];
  spelling: SpelledPitch;
  accidental?: SpelledPitch["accidental"];
};
export type ScoreToken = {
  startTick: number;
  endTick: number;
  duration: string;
  dots: number;
  pitches: ScorePitch[];
  tiePrevious: boolean;
  tieNext: boolean;
  ids: string[];
  muted: boolean;
  hiddenRest?: boolean;
};
export type ScoreVoice = { clef: "treble" | "bass"; tokens: ScoreToken[] };
export type ScoreMeasure = {
  number: number;
  startTick: number;
  endTick: number;
  numerator: number;
  denominator: number;
  sharps: number;
  voices: ScoreVoice[];
};
type Chord = {
  start: number;
  end: number;
  clef: "treble" | "bass";
  notes: TimelineNote[];
};
const letters = "cdefgab",
  natural = [0, 2, 4, 5, 7, 9, 11],
  signs = ["bb", "b", "n", "#", "##"] as const;
export function scoreFrame(song: ScoreMetadata, tick: number, rate: number) {
  let seconds = 0,
    previousTick = 0,
    microseconds = 500000;
  for (const tempo of song.tempoMap) {
    if (tempo.tick > tick) break;
    seconds += ((tempo.tick - previousTick) * microseconds) / 1e6 / song.ppq;
    previousTick = tempo.tick;
    microseconds = tempo.microseconds;
  }
  return Math.round(
    (seconds + ((tick - previousTick) * microseconds) / 1e6 / song.ppq) * rate,
  );
}
export function keyAlterations(sharps: number) {
  const result = new Map<string, number>();
  const order = sharps < 0 ? "beadgcf" : "fcgdaeb";
  for (const letter of order.slice(0, Math.min(7, Math.abs(sharps))))
    result.set(letter, sharps < 0 ? -1 : 1);
  return result;
}
function spell(midi: number, sharps: number): SpelledPitch {
  const alterations = keyAlterations(sharps);
  let best = { letter: "c", octave: 4, accidental: "n" } as SpelledPitch,
    cost = Infinity;
  for (
    let octave = Math.floor(midi / 12) - 2;
    octave <= Math.floor(midi / 12);
    octave++
  )
    for (let i = 0; i < 7; i++) {
      const alteration = midi - ((octave + 1) * 12 + natural[i]);
      if (Math.abs(alteration) > 2) continue;
      const penalty =
        Math.abs(alteration - (alterations.get(letters[i]) ?? 0)) * 4 +
        Math.abs(alteration) +
        (alteration * (sharps < 0 ? 1 : -1) > 0 ? 1 : 0);
      if (penalty < cost) {
        cost = penalty;
        best = {
          letter: letters[i] as SpelledPitch["letter"],
          octave,
          accidental: signs[alteration + 2],
        };
      }
    }
  return best;
}
const rhythms = [
  { units: 32, duration: "w", dots: 0 },
  { units: 24, duration: "h", dots: 1 },
  { units: 16, duration: "h", dots: 0 },
  { units: 12, duration: "q", dots: 1 },
  { units: 8, duration: "q", dots: 0 },
  { units: 6, duration: "8", dots: 1 },
  { units: 4, duration: "8", dots: 0 },
  { units: 3, duration: "16", dots: 1 },
  { units: 2, duration: "16", dots: 0 },
  { units: 1, duration: "32", dots: 0 },
];
export function buildScore(
  notes: TimelineNote[],
  song: ScoreMetadata,
  fromTick: number,
  toTick: number,
): ScoreMeasure[] {
  const quantum = song.ppq / 8,
    snap = (tick: number) => Math.round(tick / quantum) * quantum;
  const tracks = new Map(song.tracks.map((t) => [t.id, t]));
  const chords = new Map<string, Chord>(),
    seen = new Map<string, TimelineNote>();
  for (const n of notes) {
    if (n.startTick === undefined || n.endTick === undefined) continue;
    const start = Math.max(0, snap(n.startTick)),
      end = Math.max(start + quantum, snap(n.endTick));
    const track = tracks.get(n.trackId ?? -1);
    const clef =
      track?.staff === "treble" || track?.staff === "bass"
        ? track.staff
        : /\b(lh|left)\b/i.test(track?.title ?? "") ||
            /^piano backing$/i.test(track?.title ?? "")
          ? "bass"
          : /\b(rh|right)\b/i.test(track?.title ?? "") ||
              /^practice melody$/i.test(track?.title ?? "")
            ? "treble"
            : (n.staff ?? (n.midiNote < 60 ? "bass" : "treble"));
    const identity = `${start}:${end}:${clef}:${n.midiNote}`;
    const existing = seen.get(identity);
    if (existing) {
      if (n.highlight !== "muted") existing.highlight = n.highlight;
      continue;
    }
    const display = { ...n };
    seen.set(identity, display);
    const key = `${start}:${end}:${clef}`,
      chord = chords.get(key) ?? { start, end, clef, notes: [] };
    chord.notes.push(display);
    chords.set(key, chord);
  }
  const lanes: { clef: "treble" | "bass"; chords: Chord[]; end: number }[] = [];
  for (const chord of [...chords.values()].sort(
    (a, b) => a.start - b.start ||
      Math.max(...b.notes.map(n => n.midiNote)) - Math.max(...a.notes.map(n => n.midiNote)),
  )) {
    const pitch = (c: Chord) => c.notes.reduce((sum, n) => sum + n.midiNote, 0) / c.notes.length;
    let lane = lanes.filter((l) => l.clef === chord.clef && l.end <= chord.start)
      .sort((a, b) => Math.abs(pitch(a.chords.at(-1)!) - pitch(chord)) - Math.abs(pitch(b.chords.at(-1)!) - pitch(chord)))[0];
    if (!lane) {
      lane = { clef: chord.clef, chords: [], end: 0 };
      lanes.push(lane);
    }
    lane.chords.push(chord);
    lane.end = chord.end;
  }
  const meters = [...song.meterMap];
  if (!meters.length || meters[0].tick > 0)
    meters.unshift({ tick: 0, numerator: 4, denominator: 4 });
  const result: ScoreMeasure[] = [];
  let number = 1;
  for (let section = 0; section < meters.length; section++) {
    const meter = meters[section],
      bar = (song.ppq * 4 * meter.numerator) / meter.denominator;
    const sectionEnd = Math.min(
      song.durationTicks,
      meters[section + 1]?.tick ?? song.durationTicks,
    );
    const count = Math.ceil((sectionEnd - meter.tick) / bar);
    const first = Math.max(0, Math.floor((fromTick - meter.tick) / bar)),
      last = Math.min(count - 1, Math.floor((toTick - meter.tick - 1) / bar));
    for (let i = first; i <= last; i++) {
      const start = meter.tick + i * bar,
        end = Math.min(start + bar, sectionEnd);
      const sharps =
        song.keyMap.filter((k) => k.tick <= start).at(-1)?.sharps ?? 0;
      const measure: ScoreMeasure = {
        number: number + i,
        startTick: start,
        endTick: end,
        numerator: meter.numerator,
        denominator: meter.denominator,
        sharps,
        voices: [],
      };
      for (const clef of ["treble", "bass"] as const) {
        const active = lanes.filter(
          (l) =>
            l.clef === clef &&
            l.chords.some((c) => c.start < end && c.end > start),
        );
        for (const lane of active.length
          ? active
          : [{ clef, chords: [], end: 0 }]) {
          const tokens: ScoreToken[] = [];
          let cursor = start;
          const emit = (until: number, chord?: Chord) => {
            while (cursor < until - quantum * 0.1) {
              const units = Math.round((until - cursor) / quantum);
              const offset = Math.round((cursor - start) / quantum);
              // Rests expose beat boundaries; off-beat notes use ties across strong beats.
              const beatUnits = 32 / meter.denominator;
              const beatRemaining = beatUnits - (offset % beatUnits);
              const ceiling =
                !chord || offset % beatUnits !== 0
                  ? Math.min(units, beatRemaining)
                  : units;
              const rhythm =
                rhythms.find((r) => r.units <= ceiling) ?? rhythms.at(-1)!;
              const tokenEnd = Math.min(until, cursor + rhythm.units * quantum);
              tokens.push({
                startTick: cursor,
                endTick: tokenEnd,
                duration: rhythm.duration,
                dots: rhythm.dots,
                pitches: chord
                  ? chord.notes
                      .sort((a, b) => a.midiNote - b.midiNote)
                      .map((n) => ({
                        midi: n.midiNote,
                        highlight: n.highlight,
                        spelling: song.preserveSpelling
                          ? n.spelling
                          : spell(n.midiNote, sharps),
                      }))
                  : [],
                ids: chord ? chord.notes.map((n) => String(n.id)) : [],
                muted:
                  chord?.notes.every((n) => n.highlight === "muted") ?? false,
                tiePrevious: !!chord && cursor > chord.start,
                tieNext: !!chord && tokenEnd < chord.end,
              });
              cursor = tokenEnd;
            }
          };
          for (const chord of lane.chords.filter(
            (c) => c.start < end && c.end > start,
          )) {
            if (chord.start > cursor) emit(chord.start);
            emit(Math.min(chord.end, end), chord);
          }
          if (!lane.chords.length) {
            tokens.push({
              startTick: start,
              endTick: end,
              duration: "w",
              dots: 0,
              pitches: [],
              ids: [],
              muted: false,
              tiePrevious: false,
              tieNext: false,
            });
          } else emit(end);
          measure.voices.push({ clef, tokens });
        }
      }
      // Polyphonic stems follow register, never the order MIDI events created lanes.
      const register = (v: ScoreVoice) => {
        const pitches = v.tokens.flatMap((t) => t.pitches.map((p) => p.midi));
        return pitches.length
          ? pitches.reduce((a, b) => a + b, 0) / pitches.length
          : 0;
      };
      measure.voices.sort((a, b) =>
        a.clef === b.clef
          ? register(b) - register(a)
          : a.clef === "treble"
            ? -1
            : 1,
      );
      // Filler rests in a supplemental voice do not indicate silence while
      // another voice on this staff covers the entire interval.
      for (const voice of measure.voices) for (const token of voice.tokens) {
        if (token.pitches.length) continue;
        const sounding = measure.voices.filter(v => v !== voice && v.clef === voice.clef)
          .flatMap(v => v.tokens.filter(t => t.pitches.length))
          .sort((a, b) => a.startTick - b.startTick);
        let covered = token.startTick;
        for (const other of sounding) {
          if (other.startTick > covered) break;
          if (other.endTick > covered) covered = other.endTick;
        }
        token.hiddenRest = covered >= token.endTick;
      }
      // Accidental memory is shared between voices on a staff, but resets each bar.
      const memory = new Map<string, string>(),
        defaults = keyAlterations(sharps);
      const ordered = measure.voices
        .flatMap((v) => v.tokens.map((t) => ({ clef: v.clef, t })))
        .sort((a, b) => a.t.startTick - b.t.startTick);
      for (const { clef, t } of ordered)
        for (const p of t.pitches) {
          const key = `${clef}:${p.spelling.letter}:${p.spelling.octave}`,
            wanted = p.spelling.accidental ?? "n";
          const previous =
            memory.get(key) ??
            signs[(defaults.get(p.spelling.letter) ?? 0) + 2];
          if (wanted !== previous && !t.tiePrevious) p.accidental = wanted;
          memory.set(key, wanted);
        }
      result.push(measure);
    }
    number += count;
  }
  return result;
}
