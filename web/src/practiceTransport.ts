export type PracticeHand = "both" | "left" | "right";
export const practiceAction = (running: boolean, paused: boolean) =>
  running ? "practice.pause" : paused ? "practice.resume" : "practice.start";
export const toggledHand = (
  hand: PracticeHand,
  button: "left" | "right",
): PracticeHand =>
  hand === "both"
    ? button === "left"
      ? "right"
      : "left"
    : hand === button
      ? hand
      : "both";
