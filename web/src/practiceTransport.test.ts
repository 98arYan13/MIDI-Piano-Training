import { describe, expect, it } from "vitest";
import { practiceAction, toggledHand } from "./practiceTransport";

describe("practice controls", () => {
  it("resumes a paused exercise rather than resetting it", () => {
    expect(practiceAction(true, false)).toBe("practice.pause");
    expect(practiceAction(false, true)).toBe("practice.resume");
    expect(practiceAction(false, false)).toBe("practice.start");
  });
  it("toggles either hand while keeping at least one enabled", () => {
    expect(toggledHand("both", "left")).toBe("right");
    expect(toggledHand("both", "right")).toBe("left");
    expect(toggledHand("left", "right")).toBe("both");
    expect(toggledHand("right", "left")).toBe("both");
    expect(toggledHand("left", "left")).toBe("left");
    expect(toggledHand("right", "right")).toBe("right");
  });
});
