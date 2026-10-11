import { Accidental, StaveNote } from "vexflow/bravura";
import type { TimelineNote } from "./notationModel";
// Native spelling owns the staff position; MIDI pitch only owns sound/scoring.
export function notationNote(chord: TimelineNote[], clef: "treble" | "bass") {
  const note = new StaveNote({
    clef,
    keys: chord.map(
      (n) =>
        `${n.spelling.letter}${n.spelling.accidental === "n" ? "" : (n.spelling.accidental ?? "")}/${n.spelling.octave}`,
    ),
    duration: chord[0].duration ?? "q",
  });
  chord.forEach((n, i) => {
    if (n.spelling.accidental)
      note.addModifier(new Accidental(n.spelling.accidental), i);
  });
  return note;
}
