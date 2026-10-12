import { toggledHand, type PracticeHand } from "./practiceTransport";

export function PlayerControls({
  hand,
  onHand,
  playing,
  playLabel,
  onPlay,
}: {
  hand: PracticeHand;
  onHand: (hand: PracticeHand) => void;
  playing: boolean;
  playLabel: string;
  onPlay: () => void;
}) {
  return (
    <div className="practice-transport" aria-label="Player controls">
      {(["left", "right"] as const).map((side) => (
        <button
          key={side}
          className={`hand-control ${side}-hand`}
          aria-label={side === "left" ? "Left hand" : "Right hand"}
          aria-pressed={hand === "both" || hand === side}
          disabled={hand === side}
          title={`Toggle ${side}-hand practice`}
          onClick={() => onHand(toggledHand(hand, side))}
        >
          <svg viewBox="0 0 32 36" aria-hidden="true" fill="currentColor">
            <path d="M10 18V7a2 2 0 0 1 4 0v10h1V4a2 2 0 0 1 4 0v13h1V6a2 2 0 0 1 4 0v12h1V10a2 2 0 0 1 4 0v13c0 7-4 11-10 11h-2c-4 0-7-2-9-6L3 19a2.4 2.4 0 0 1 4-2l3 4z" />
          </svg>
          <span>{side === "left" ? "Left hand" : "Right hand"}</span>
        </button>
      ))}
      <button
        className="practice-play"
        aria-label={playLabel}
        title={playLabel}
        onClick={onPlay}
      >
        <svg viewBox="0 0 24 24" aria-hidden="true">
          <path d={playing ? "M6 4h4v16H6zm8 0h4v16h-4z" : "M7 3v18l15-9z"} />
        </svg>
      </button>
    </div>
  );
}

export function RestartButton({
  label,
  onClick,
}: {
  label: string;
  onClick: () => void;
}) {
  return (
    <button
      className="practice-restart"
      aria-label={label}
      title={label}
      onClick={onClick}
    >
      <svg
        viewBox="0 0 24 24"
        aria-hidden="true"
        fill="none"
        stroke="currentColor"
        strokeWidth="2"
        strokeLinecap="round"
        strokeLinejoin="round"
      >
        <path d="M20 7H4l4-4M4 17h16l-4 4M20 7v4M4 17v-4" />
      </svg>
    </button>
  );
}
