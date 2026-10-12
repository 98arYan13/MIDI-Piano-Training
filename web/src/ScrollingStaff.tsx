import { memo, useEffect, useRef, useState } from "react";
import {
  Barline,
  Renderer,
  ModifierContext,
  Stave,
  StaveConnector,
  TickContext,
} from "vexflow/bravura";
import { formatNotationTick, notationNote } from "./notationNote";
import {
  frameX,
  staffClefs,
  segmentFingerprint,
  noteClef,
  type StaffMode,
  type TimelineNote,
} from "./notationModel";

const segmentWidth = 480;
const colors = {
  target: "#dcb77d",
  hit: "#68c9ad",
  miss: "#ef9689",
  muted: "#85918a",
};
// SVG segments are stable while the native frame changes. Only the track transform moves.
const Segment = memo(
  function Segment({
    mode,
    notes,
    index,
    sampleRate,
    spacing,
    header = false,
    paper = false,
  }: {
    mode: StaffMode;
    notes: TimelineNote[];
    index: number;
    sampleRate: number;
    spacing: number;
    header?: boolean;
    paper?: boolean;
  }) {
    const host = useRef<HTMLDivElement>(null);
    useEffect(() => {
      const el = host.current;
      if (!el) return;
      el.replaceChildren();
      const renderer = new Renderer(el, Renderer.Backends.SVG);
      renderer.resize(
        header ? 106 : segmentWidth,
        mode === "grand" ? 236 : 174,
      );
      const context = renderer.getContext();
      context.setFillStyle(paper ? "#202124" : "#c9d0c8");
      context.setStrokeStyle(paper ? "#55585d" : "#69746d");
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
          const clef = noteClef(n, mode);
          if (mode !== "grand" && clef !== mode) continue;
          const x =
              frameX(n.startFrame, sampleRate, spacing) - index * segmentWidth,
            end =
              frameX(n.endFrame, sampleRate, spacing) - index * segmentWidth;
          if (end < 0 || x >= segmentWidth) continue;
          const key = `${n.startFrame}:${clef}:${n.duration ?? "q"}:${n.highlight ?? ""}`;
          groups.set(key, [...(groups.get(key) ?? []), n]);
        }
        const onsets = new Map<string, { modifiers: ModifierContext; tick: TickContext }>();
        const rendered = Array.from(groups.values(), chord => {
          const first = chord[0],
            clef = noteClef(first, mode),
            stave = staves[mode === "grand" && clef === "bass" ? 1 : 0];
          const key = `${first.startFrame}:${clef}`;
          let onset = onsets.get(key);
          if (!onset) {
            onset = { modifiers: new ModifierContext(), tick: new TickContext() };
            onsets.set(key, onset);
          }
          const note = notationNote(chord, clef, onset.modifiers).setStave(stave);
          onset.tick.addTickable(note);
          return { chord, note, onset };
        });
        const positioned = new Set<TickContext>();
        for (const { chord, note, onset } of rendered) {
          const first = chord[0];
          const x =
            frameX(first.startFrame, sampleRate, spacing) -
            index * segmentWidth;
          if (!positioned.has(onset.tick)) {
            const tick = formatNotationTick(onset.tick).setX(x);
            tick.setX(x - (note.getAbsoluteX() - x) + note.getXShift() - note.getGlyphWidth() / 2);
            positioned.add(tick);
          }
          const ink = paper
            ? {
                target: "#202124",
                hit: "#28765d",
                miss: "#c14e3b",
                muted: "#a9afb8",
              }
            : colors;
          note.setStyle({
            fillStyle: ink[first.highlight ?? "target"],
            strokeStyle: ink[first.highlight ?? "target"],
          });
          if (x >= 0) note.setContext(context).draw();
          if (!paper)
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
    }, [mode, notes, index, sampleRate, spacing, header, paper]);
    return (
      <div
        ref={host}
        className={header ? "staff-clefs" : "staff-segment"}
        style={header ? undefined : { left: index * segmentWidth }}
        data-clefs={staffClefs(mode).join(" ")}
      />
    );
  },
  (before, after) => {
    if (
      before.mode !== after.mode ||
      before.index !== after.index ||
      before.sampleRate !== after.sampleRate ||
      before.spacing !== after.spacing ||
      before.header !== after.header ||
      before.paper !== after.paper
    )
      return false;
    if (after.header || before.notes === after.notes) return true;
    return (
      segmentFingerprint(
        before.notes,
        before.index,
        before.sampleRate,
        before.spacing,
        before.mode,
      ) ===
      segmentFingerprint(
        after.notes,
        after.index,
        after.sampleRate,
        after.spacing,
        after.mode,
      )
    );
  },
);

export type ScrollingStaffProps = {
  mode: StaffMode;
  notes: TimelineNote[];
  sampleRate: number;
  clockFrame: () => number;
  pixelsPerSecond?: number;
  running?: boolean;
  label?: string;
  paper?: boolean;
  tempo?: number;
  measures?: { number: number; startFrame: number }[];
  scrub?: {
    durationFrames: number;
    start: () => void;
    seek: (frame: number) => Promise<void>;
  };
};
export function ScrollingStaff({
  mode,
  notes,
  sampleRate,
  clockFrame,
  pixelsPerSecond = 110,
  running = true,
  label = "Practice notation",
  paper = false,
  tempo = 100,
  measures,
  scrub,
}: ScrollingStaffProps) {
  const viewport = useRef<HTMLDivElement>(null),
    track = useRef<HTMLDivElement>(null);
  const [windowRange, setWindowRange] = useState({ first: 0, last: 3 });
  const [dragFrame, setDragFrame] = useState<number | null>(null);
  const drag = useRef<{ pointer: number; x: number; frame: number; moved: boolean } | null>(null);
  const clock = useRef(clockFrame);
  clock.current = () => dragFrame ?? clockFrame();
  const stoppedFrame = running ? undefined : (dragFrame ?? clockFrame());
  useEffect(() => {
    let raf = 0,
      previous = 0;
    const reduced = matchMedia("(prefers-reduced-motion: reduce)");
    const draw = (now: number) => {
      if (!reduced.matches || now - previous >= 150) {
        previous = now;
        const x = frameX(clock.current(), sampleRate, pixelsPerSecond),
          width = viewport.current?.clientWidth ?? 800;
        const anchor = paper ? width / 2 : Math.min(180, width * 0.28);
        if (track.current)
          track.current.style.transform = `translate3d(${anchor - x}px,0,0)`;
        const first = Math.max(0, Math.floor((x - anchor) / segmentWidth) - 1),
          last = Math.max(first + 2, Math.ceil((x + width) / segmentWidth) + 1);
        setWindowRange((old) =>
          old.first === first && old.last === last ? old : { first, last },
        );
      }
    };
    const animate = (now: number) => {
      draw(now);
      if (running) raf = requestAnimationFrame(animate);
    };
    animate(performance.now());
    const resize = new ResizeObserver(() => draw(performance.now()));
    if (viewport.current) resize.observe(viewport.current);
    return () => {
      cancelAnimationFrame(raf);
      resize.disconnect();
    };
  }, [sampleRate, pixelsPerSecond, running, stoppedFrame, notes, paper, dragFrame]);
  const measureWidth = (240 / tempo) * pixelsPerSecond;
  const firstMeasure = Math.max(
    0,
    Math.floor((windowRange.first * segmentWidth) / measureWidth),
  );
  const lastMeasure = Math.ceil(
    ((windowRange.last + 1) * segmentWidth) / measureWidth,
  );
  const visibleMeasures =
    measures ??
    Array.from({ length: lastMeasure - firstMeasure + 1 }, (_, i) => ({
      number: firstMeasure + i + 1,
      startFrame: (((firstMeasure + i) * 240) / tempo) * sampleRate,
    }));
  return (
    <div
      className={`scrolling-staff ${mode}${paper ? " paper-staff" : ""}${scrub ? " draggable-staff" : ""}${dragFrame !== null ? " dragging-staff" : ""}`}
      ref={viewport}
      role="img"
      aria-label={`${label}, ${mode === "grand" ? "joined treble and bass staves" : mode + " clef"}`}
      onPointerDown={(e) => {
        if (!scrub || e.button !== 0 || drag.current) return;
        e.preventDefault();
        e.currentTarget.setPointerCapture(e.pointerId);
        drag.current = { pointer: e.pointerId, x: e.clientX, frame: clockFrame(), moved: false };
      }}
      onPointerMove={(e) => {
        const d = drag.current;
        if (!scrub || !d || d.pointer !== e.pointerId) return;
        if (!d.moved && Math.abs(e.clientX - d.x) < 4) return;
        if (!d.moved) { d.moved = true; scrub.start(); }
        setDragFrame(Math.max(0, Math.min(scrub.durationFrames,
          d.frame - (e.clientX - d.x) * sampleRate / pixelsPerSecond)));
      }}
      onPointerUp={(e) => {
        const d = drag.current;
        if (!scrub || !d || d.pointer !== e.pointerId) return;
        drag.current = null;
        e.currentTarget.releasePointerCapture(e.pointerId);
        if (d.moved) {
          const frame = Math.max(0, Math.min(scrub.durationFrames,
            d.frame - (e.clientX - d.x) * sampleRate / pixelsPerSecond));
          void scrub.seek(frame).finally(() => setDragFrame(null));
        }
      }}
      onLostPointerCapture={() => {
        if (drag.current) { drag.current = null; setDragFrame(null); }
      }}
      onPointerCancel={() => { drag.current = null; setDragFrame(null); }}
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
              y1={
                (mode === "grand" ? 55 : 75) +
                i * 92 +
                line * 10 +
                (paper ? 40 : 0)
              }
              y2={
                (mode === "grand" ? 55 : 75) +
                i * 92 +
                line * 10 +
                (paper ? 40 : 0)
              }
              stroke={paper ? "#73777c" : "#69746d"}
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
            paper={paper}
          />
        ))}
        {paper &&
          visibleMeasures.map((measure) => (
            <div
              key={`measure-${measure.number}`}
              className="staff-measure"
              style={{
                left:
                  frameX(measure.startFrame, sampleRate, pixelsPerSecond) - 28,
                top: mode === "grand" ? 95 : 115,
                height: mode === "grand" ? 132 : 40,
              }}
            >
              <span>{measure.number}</span>
            </div>
          ))}
        {paper && (
          <Segment
            mode={mode}
            notes={notes}
            index={0}
            sampleRate={sampleRate}
            spacing={pixelsPerSecond}
            header
            paper
          />
        )}
      </div>
      {!paper && (
        <Segment
          mode={mode}
          notes={notes}
          index={0}
          sampleRate={sampleRate}
          spacing={pixelsPerSecond}
          header
        />
      )}
      <div className="staff-playhead">{!paper && <span>NOW</span>}</div>
    </div>
  );
}
