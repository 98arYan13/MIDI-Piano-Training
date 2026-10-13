import { expect, it } from "vitest";
import { showFingering } from "./fingeringDisplay";
import type { NoteFingering, FingeringTip } from "./notationModel";
const tips: FingeringTip[] = ["phraseStart", "thumbCrossing", "positionChange", "majorPositionChange", "difficultChord"];
it("uses exact persisted category subsets in every density, including manual fingers", () => {
  for (let mask = 0; mask < 32; mask++) {
    const selected = tips.filter((_, i) => mask & (1 << i));
    const f: NoteFingering = { finger: 3, hand: "right", source: "manual", tips: selected };
    expect(showFingering(f, "full", false)).toBe(true);
    expect(showFingering(f, "medium", false)).toBe(selected.some(t => ["phraseStart", "thumbCrossing", "positionChange", "difficultChord"].includes(t)));
    expect(showFingering(f, "pro", false)).toBe(selected.some(t => ["thumbCrossing", "majorPositionChange"].includes(t)));
    expect(showFingering(f, "full", true)).toBe(false);
    expect(showFingering({ ...f, finger: null }, "full", false)).toBe(false);
  }
  expect(showFingering(undefined, "full", false)).toBe(false);
});
