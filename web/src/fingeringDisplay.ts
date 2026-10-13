import type { FingeringMode, NoteFingering } from "./notationModel";
export function showFingering(f: NoteFingering | undefined, mode: FingeringMode, tiePrevious: boolean): boolean {
  if (!f || f.finger === null || tiePrevious) return false;
  if (mode === "full") return true;
  const categories = mode === "pro" ? ["thumbCrossing", "majorPositionChange"] : ["phraseStart", "thumbCrossing", "positionChange", "difficultChord"];
  return f.tips.some(t => categories.includes(t));
}
export function mergeFingering(a: NoteFingering | undefined, b: NoteFingering | undefined): NoteFingering | undefined {
  if (!a) return b;
  if (!b) return a;
  const tips = [...new Set([...a.tips, ...b.tips])];
  if (a.finger !== b.finger || a.hand !== b.hand) return { ...a, finger: null, tips, reason: "Conflicting duplicate unison fingerings" };
  return { ...a, tips, source: a.source === "manual" || b.source === "manual" ? "manual" : "generated" };
}
