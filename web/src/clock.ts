import type { SnapshotMessage } from "./types";
export function returnPosition(animation: { from: number; to: number; startFrame: number; durationFrames: number }, frame: number) {
  const t = Math.max(0, Math.min(1, (frame-animation.startFrame)/animation.durationFrames));
  return animation.from+(animation.to-animation.from)*t*t*(3-2*t);
}
// A native process owns its epoch. Reconnects receive a complete snapshot.
export function acceptSnapshot(
  current: SnapshotMessage | null,
  next: SnapshotMessage,
): boolean {
  return (
    !current ||
    next.generation > current.generation ||
    (next.generation === current.generation &&
      next.state.sampleFrame >= current.state.sampleFrame)
  );
}
// Bound extrapolation when the service stops responding; this clock is visual only.
export function estimateFrame(
  snapshot: SnapshotMessage,
  receivedMs: number,
  nowMs: number,
): number {
  return (
    snapshot.state.sampleFrame +
    (snapshot.state.audioReady
      ? (Math.max(0, Math.min(250, nowMs - receivedMs)) *
          snapshot.state.sampleRate) /
        1000
      : 0)
  );
}
export function clockOffset(
  sentMs: number,
  receivedMs: number,
  engineTimeNs: number,
): number {
  return engineTimeNs / 1e6 - (sentMs + receivedMs) / 2;
}
