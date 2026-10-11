import { memo, useEffect, useRef, useState } from "react";
import {
  Barline,
  Renderer,
  Stave,
  StaveConnector,
  TickContext,
} from "vexflow/bravura";
import { notationNote } from "./notationNote";
import {
  frameX,
  staffClefs,
  segmentFingerprint,
  noteClef,
  type StaffMode,
  type TimelineNote,
} from "./notationModel";

const segmentWidth = 480;
const colors = { target: "#dcb77d", hit: "#68c9ad", miss: "#ef9689" };
// SVG segments are stable while the native frame changes. Only the track transform moves.
const Segment = memo(function Segment({
  mode,
  notes,
  index,
  sampleRate,
  spacing,
  header = false,
}: {
  mode: StaffMode;
  notes: TimelineNote[];
  index: number;
  sampleRate: number;
  spacing: number;
  header?: boolean;
}) {
  const host = useRef<HTMLDivElement>(null);
  useEffect(() => {
    const el = host.current;
    if (!el) return;
    el.replaceChildren();
    const renderer = new Renderer(el, Renderer.Backends.SVG);
    renderer.resize(header ? 106 : segmentWidth, mode === "grand" ? 236 : 174);
    const context = renderer.getContext();
    context.setFillStyle("#c9d0c8");
    context.setStrokeStyle("#69746d");
    const staves = staffClefs(mode).map((clef, i) => {
      const stave = new Stave(
        header ? 16 : 0,
        (mode === "grand" ? 15 : 35) + i * 92,
        header ? 100 : segmentWidth + 1,
      )
        .setBegBarType(Barline.type.NONE)
        .setEndBarType(Barline.type.NONE);
      if (header) {
        stave.addClef(clef);
        stave.setContext(context).draw();
      } else stave.setContext(context);
      return stave;
    });
    if (header && mode === "grand") {
      new StaveConnector(staves[0], staves[1])
        .setType(StaveConnector.type.BRACE)
        .setContext(context)
        .draw();
      new StaveConnector(staves[0], staves[1])
        .setType(StaveConnector.type.SINGLE_LEFT)
        .setContext(context)
        .draw();
    }
    if (!header) {
      const groups = new Map<string, TimelineNote[]>();
      for (const n of notes) {
        const clef = noteClef(n,mode);
        if (mode !== "grand" && clef !== mode) continue;
        const x =
            frameX(n.startFrame, sampleRate, spacing) - index * segmentWidth,
          end = frameX(n.endFrame, sampleRate, spacing) - index * segmentWidth;
        if (end < 0 || x >= segmentWidth) continue;
        const key = `${n.startFrame}:${clef}:${n.duration ?? "q"}:${n.highlight ?? ""}`;
        groups.set(key, [...(groups.get(key) ?? []), n]);
      }
      for (const chord of groups.values()) {
        const first = chord[0],
          clef = noteClef(first,mode),
          stave = staves[mode === "grand" && clef === "bass" ? 1 : 0];
        const note = notationNote(chord, clef);
        const x =
          frameX(first.startFrame, sampleRate, spacing) - index * segmentWidth;
        note.setStave(stave);
        const tick = new TickContext().addTickable(note).preFormat().setX(x);
        tick.setX(x - (note.getAbsoluteX() - x) - note.getGlyphWidth() / 2);
        note.setStyle({
          fillStyle: colors[first.highlight ?? "target"],
          strokeStyle: colors[first.highlight ?? "target"],
        });
        if (x >= 0) note.setContext(context).draw();
        chord.forEach((n, i) => {
          context.setStrokeStyle(colors[n.highlight ?? "target"]);
          context.setLineWidth(3);
          context.beginPath();
          context.moveTo(Math.max(0, x + 16), note.getYs()[i] + 7);
          context.lineTo(
            Math.min(
              segmentWidth,
              x +
                Math.max(
                  18,
                  frameX(n.endFrame - n.startFrame, sampleRate, spacing),
                ),
            ),
            note.getYs()[i] + 7,
          );
          context.stroke();
        });
      }
    }
    const svg = el.querySelector("svg");
    svg?.setAttribute("aria-hidden", "true");
  }, [mode, notes, index, sampleRate, spacing, header]);
  return (
    <div
      ref={host}
      className={header ? "staff-clefs" : "staff-segment"}
      style={header ? undefined : { left: index * segmentWidth }}
      data-clefs={staffClefs(mode).join(" ")}
    />
  );
}, (before, after) => {
  if(before.mode!==after.mode || before.index!==after.index || before.sampleRate!==after.sampleRate || before.spacing!==after.spacing || before.header!==after.header)return false;
  if(after.header || before.notes===after.notes)return true;
  return segmentFingerprint(before.notes,before.index,before.sampleRate,before.spacing,before.mode)===segmentFingerprint(after.notes,after.index,after.sampleRate,after.spacing,after.mode);
});

export type ScrollingStaffProps = {
  mode: StaffMode;
  notes: TimelineNote[];
  sampleRate: number;
  clockFrame: () => number;
  pixelsPerSecond?: number;
  running?: boolean;
  label?: string;
};
export function ScrollingStaff({
  mode,
  notes,
  sampleRate,
  clockFrame,
  pixelsPerSecond = 110,
  running = true,
  label = "Practice notation",
}: ScrollingStaffProps) {
  const viewport = useRef<HTMLDivElement>(null),
    track = useRef<HTMLDivElement>(null);
  const [windowRange, setWindowRange] = useState({ first: 0, last: 3 });
  const clock = useRef(clockFrame);
  clock.current = clockFrame;
  const stoppedFrame = running ? undefined : clockFrame();
  useEffect(() => {
    let raf = 0,
      previous = 0;
    const reduced = matchMedia("(prefers-reduced-motion: reduce)");
    const draw = (now: number) => {
      if (!reduced.matches || now - previous >= 150) {
        previous = now;
        const x = frameX(clock.current(), sampleRate, pixelsPerSecond),
          width = viewport.current?.clientWidth ?? 800;
        const anchor = Math.min(180, width * 0.28);
        if (track.current)
          track.current.style.transform = `translate3d(${anchor - x}px,0,0)`;
        const first = Math.max(0, Math.floor((x - anchor) / segmentWidth) - 1),
          last = Math.max(first + 2, Math.ceil((x + width) / segmentWidth) + 1);
        setWindowRange((old) =>
          old.first === first && old.last === last ? old : { first, last },
        );
      }
      if (running) raf = requestAnimationFrame(draw);
    };
    draw(performance.now());
    return () => cancelAnimationFrame(raf);
  }, [sampleRate, pixelsPerSecond, running, stoppedFrame, notes]);
  return (
    <div
      className={`scrolling-staff ${mode}`}
      ref={viewport}
      role="img"
      aria-label={`${label}, ${mode === "grand" ? "joined treble and bass staves" : mode + " clef"}`}
    >
      <svg
        className="staff-lines"
        width="100%"
        height="100%"
        aria-hidden="true"
      >
        {staffClefs(mode).flatMap((_, i) =>
          Array.from({ length: 5 }, (_, line) => (
            <line
              key={`${i}:${line}`}
              x1="0"
              x2="100%"
              y1={(mode === "grand" ? 55 : 75) + i * 92 + line * 10}
              y2={(mode === "grand" ? 55 : 75) + i * 92 + line * 10}
              stroke="#69746d"
              strokeWidth="1"
            />
          )),
        )}
      </svg>
      <div className="staff-track" ref={track}>
        {Array.from(
          { length: windowRange.last - windowRange.first + 1 },
          (_, i) => i + windowRange.first,
        ).map((index) => (
          <Segment
            key={index}
            mode={mode}
            notes={notes}
            index={index}
            sampleRate={sampleRate}
            spacing={pixelsPerSecond}
          />
        ))}
      </div>
      <Segment
        mode={mode}
        notes={notes}
        index={0}
        sampleRate={sampleRate}
        spacing={pixelsPerSecond}
        header
      />
      <div className="staff-playhead">
        <span>NOW</span>
      </div>
    </div>
  );
}
