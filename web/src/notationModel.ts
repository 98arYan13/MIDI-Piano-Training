export type StaffMode = "treble" | "bass" | "grand";
export type SpelledPitch = {
  letter: "c" | "d" | "e" | "f" | "g" | "a" | "b";
  accidental?: "n" | "#" | "b" | "##" | "bb";
  octave: number;
};
export type TimelineNote = {
  id: string;
  midiNote: number;
  spelling: SpelledPitch;
  startFrame: number;
  endFrame: number;
  staff?: "treble" | "bass";
  duration?: "w" | "h" | "q" | "8" | "16";
  highlight?: "target" | "hit" | "miss";
};
export const staffClefs = (mode: StaffMode): ("treble" | "bass")[] =>
  mode === "grand" ? ["treble", "bass"] : [mode];
export const frameX = (
  frame: number,
  sampleRate: number,
  pixelsPerSecond: number,
) => (frame / Math.max(1, sampleRate)) * pixelsPerSecond;
