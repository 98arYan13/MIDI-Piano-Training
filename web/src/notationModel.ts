export type FingeringTip = "phraseStart" | "thumbCrossing" | "positionChange" | "majorPositionChange" | "difficultChord";
export type FingeringMode = "full" | "medium" | "pro";
export type NoteFingering = { finger: 1 | 2 | 3 | 4 | 5 | null; hand: "left" | "right"; source: "generated" | "manual"; tips: FingeringTip[]; reason?: string; automaticFinger?: 1 | 2 | 3 | 4 | 5 | null };
export type StaffMode = "treble" | "bass" | "grand";
export type SpelledPitch = {
  letter: "c" | "d" | "e" | "f" | "g" | "a" | "b";
  accidental?: "n" | "#" | "b" | "##" | "bb";
  octave: number;
};
export type TimelineNote = {
  fingering?: NoteFingering;
  id: string;
  startTick?: number;
  endTick?: number;
  trackId?: number;
  midiNote: number;
  spelling: SpelledPitch;
  startFrame: number;
  endFrame: number;
  staff?: "treble" | "bass";
  duration?: "w" | "h" | "q" | "8" | "16";
  highlight?: "target" | "hit" | "miss" | "muted";
};
export const spelledLabel = (pitch: SpelledPitch) =>
  `${pitch.letter.toUpperCase()}${pitch.accidental === 'n' ? '' : pitch.accidental ?? ''}${pitch.octave}`;
export const staffClefs = (mode: StaffMode): ("treble" | "bass")[] =>
  mode === "grand" ? ["treble", "bass"] : [mode];
export const noteClef = (note: TimelineNote, mode: StaffMode): "treble" | "bass" =>
  note.staff ?? (mode === "grand" ? (note.midiNote < 60 ? "bass" : "treble") : mode);
export const frameX = (
  frame: number,
  sampleRate: number,
  pixelsPerSecond: number,
) => (frame / Math.max(1, sampleRate)) * pixelsPerSecond;
export function segmentFingerprint(notes: TimelineNote[], index: number, sampleRate: number, spacing: number, mode: StaffMode) {
  return JSON.stringify(notes.filter(n => {
    const clef=noteClef(n,mode);
    return (mode==='grand'||mode===clef) && frameX(n.endFrame,sampleRate,spacing)>=index*480 && frameX(n.startFrame,sampleRate,spacing)<(index+1)*480;
  }));
}
