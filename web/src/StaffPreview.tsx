import { useRef, useState } from "react";
import { useNative } from "./native";
import { ScrollingStaff } from "./ScrollingStaff";
import type { StaffMode, TimelineNote } from "./notationModel";
const previewNotes: TimelineNote[] = [
  {
    id: "bass-c",
    midiNote: 48,
    spelling: { letter: "c", octave: 3 },
    staff: "bass",
    startFrame: 0,
    endFrame: 96000,
    duration: "h",
  },
  {
    id: "c",
    midiNote: 60,
    spelling: { letter: "c", octave: 4 },
    startFrame: 0,
    endFrame: 48000,
  },
  {
    id: "eb",
    midiNote: 63,
    spelling: { letter: "e", accidental: "b", octave: 4 },
    startFrame: 48000,
    endFrame: 96000,
  },
  {
    id: "g",
    midiNote: 67,
    spelling: { letter: "g", octave: 4 },
    startFrame: 96000,
    endFrame: 144000,
  },
  {
    id: "low",
    midiNote: 33,
    spelling: { letter: "a", octave: 1 },
    staff: "bass",
    startFrame: 144000,
    endFrame: 192000,
  },
  {
    id: "high",
    midiNote: 84,
    spelling: { letter: "c", octave: 6 },
    startFrame: 144000,
    endFrame: 192000,
  },
  {
    id: "sharp",
    midiNote: 66,
    spelling: { letter: "f", accidental: "#", octave: 4 },
    startFrame: 192000,
    endFrame: 240000,
  },
];
export function StaffPreview() {
  const native = useNative(),
    [mode, setMode] = useState<StaffMode>("treble"),
    [scroll, setScroll] = useState(false);
  const origin = useRef(0);
  return (
    <div className="staff-preview">
      <div className="staff-preview-tools">
        <label>
          Staff view{" "}
          <select
            aria-label="Staff view"
            value={mode}
            onChange={(e) => setMode(e.target.value as StaffMode)}
          >
            <option value="treble">Treble</option>
            <option value="bass">Bass</option>
            <option value="grand">Grand</option>
          </select>
        </label>
        <button
          aria-pressed={scroll}
          onClick={() => {
            origin.current = native.frame();
            setScroll(!scroll);
          }}
        >
          {scroll ? "Freeze preview" : "Preview scroll"}
        </button>
        <span>Notation preview · no example audio</span>
      </div>
      <ScrollingStaff
        mode={mode}
        notes={previewNotes}
        sampleRate={48000}
        clockFrame={() =>
          scroll
            ? ((native.frame() - origin.current) * 48000) /
              (native.state?.sampleRate ?? 48000)
            : 0
        }
      />
      <p className="helper">
        Middle C, accidentals, ledger lines and duration bars. Native exercise
        generation and scoring are added in the next implementation step.
      </p>
    </div>
  );
}
