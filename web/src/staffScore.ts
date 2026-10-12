import type { TimelineNote } from "./notationModel";
import type { ScoreMetadata } from "./scoreModel";

// Staff already supplies authoritative spellings and a native quarter-note timeline.
export function staffScore(
  notes: TimelineNote[],
  key: string,
  tempo: number,
  sampleRate: number,
) {
  const ppq = 384;
  const ticks = (frame: number) =>
    Math.round((frame * tempo * ppq) / (60 * sampleRate));
  const display = notes.map((n) => ({
    ...n,
    startTick: ticks(n.startFrame),
    endTick: ticks(n.endFrame),
  }));
  const fifths: Record<string, number> = {
    C: 0,
    G: 1,
    D: 2,
    A: 3,
    E: 4,
    B: 5,
    F: -1,
    Bb: -2,
    Eb: -3,
    Ab: -4,
    Db: -5,
    Gb: -6,
  };
  const song: ScoreMetadata = {
    ppq,
    durationTicks: Math.max(ppq * 8, ...display.map((n) => n.endTick)),
    tempoMap: [{ tick: 0, microseconds: 60000000 / tempo }],
    meterMap: [{ tick: 0, numerator: 4, denominator: 4 }],
    keyMap: [{ tick: 0, sharps: fifths[key] ?? 0 }],
    tracks: [],
    preserveSpelling: true,
  };
  return { notes: display, song };
}
