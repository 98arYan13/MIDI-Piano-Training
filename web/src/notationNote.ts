import {
  Accidental,
  ModifierContext,
  StaveNote,
  TickContext,
} from "vexflow/bravura";
import type { TimelineNote } from "./notationModel";
// Native spelling owns the staff position; MIDI pitch only owns sound/scoring.
export function notationNote(
  chord: TimelineNote[],
  clef: "treble" | "bass",
  modifiers = new ModifierContext(),
) {
  const note = new StaveNote({
    clef,
    autoStem: true,
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
  // Timeline placement bypasses Formatter; still format chord/accidental collisions.
  return note.addToModifierContext(modifiers);
}

export function formatNotationTick(tick: TickContext) {
  tick.preFormat();
  const voices = tick
    .getTickables()
    .filter((note): note is StaveNote => note instanceof StaveNote);
  separateNotationVoices(voices);
  return tick;
}

export function separateNotationVoices(voices: StaveNote[]) {
  // VexFlow's whole-note voice layout compares the lowest chord pitch only.
  // Check every head so a melody cannot sit inside an upper chord tone.
  voices.forEach((voice, index) => {
    for (const previous of voices.slice(0, index)) {
      const closePitch = voice
        .getYs()
        .some((y) =>
          previous.getYs().some((other) => Math.abs(y - other) < 10),
        );
      const clearance =
        Math.max(voice.getVoiceShiftWidth(), previous.getVoiceShiftWidth()) + 2;
      if (
        closePitch &&
        Math.abs(voice.getXShift() - previous.getXShift()) < clearance
      )
        voice.setXShift(previous.getXShift() + clearance);
    }
  });
}
