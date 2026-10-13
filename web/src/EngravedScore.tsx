import { showFingering } from "./fingeringDisplay";
import { useEffect, useMemo, useRef, useState } from "react";
import {
  Accidental,
  Barline,
  Beam,
  Dot,
  Formatter,
  FretHandFinger,
  Modifier,
  Fraction,
  Renderer,
  Stave,
  StaveConnector,
  StaveNote,
  StaveTie,
  Voice,
} from "vexflow/bravura";
import {
  buildScore,
  scoreFrame,
  scoreTick,
  type ScoreMetadata,
  type ScoreToken,
} from "./scoreModel";
import { staffClefs, spelledLabel, type FingeringMode, type StaffMode, type TimelineNote } from "./notationModel";
import type { ScrollingStaffProps } from "./ScrollingStaff";
import { separateNotationVoices } from "./notationNote";
import { LoopMarkers, type LoopSelection } from "./LoopMarkers";
// Keep VexFlow's text/font and modifier positioning; only reserve chord/stem clearance.
class ScoreFinger extends FretHandFinger {
  draw() {
    const note = this.checkAttachedNote() as StaveNote;
    const ys = note.getYs(), index = this.getIndex() ?? 0;
    const stem = note.hasStem() ? note.getStemExtents() : { topY: Math.min(...ys), baseY: Math.max(...ys) };
    const right = this.getPosition() === Modifier.Position.ABOVE;
    const target = right
      ? Math.min(...ys, stem.topY, stem.baseY) - 16 - index * 14
      : Math.max(...ys, stem.topY, stem.baseY) + 22 + (ys.length - index - 1) * 14;
    // FretHandFinger's baseline offsets are -7 above and +15 below the notehead.
    this.setOffsetY(target - ys[index] - (right ? -7 : 15));
    const context = this.checkContext();
    context.openGroup("score-finger", this.getAttribute("id"));
    super.draw();
    context.closeGroup();
  }
}
const keyNames = [
  "Cb",
  "Gb",
  "Db",
  "Ab",
  "Eb",
  "Bb",
  "F",
  "C",
  "G",
  "D",
  "A",
  "E",
  "B",
  "F#",
  "C#",
];
type Point = { frame: number; x: number };
export function interpolateScore(
  points: Point[],
  value: number,
  inverse = false,
) {
  if (!points.length) return 0;
  const input = (p: Point) => (inverse ? p.x : p.frame),
    output = (p: Point) => (inverse ? p.frame : p.x);
  let lo = 0,
    hi = points.length - 1;
  while (lo < hi) {
    const mid = Math.ceil((lo + hi) / 2);
    if (input(points[mid]) <= value) lo = mid;
    else hi = mid - 1;
  }
  if (inverse && lo === points.length - 1 && points.length > 1) lo--;
  const a = points[lo],
    b = points[Math.min(lo + 1, points.length - 1)];
  if (input(a) === input(b)) return output(a);
  const fraction = (value - input(a)) / (input(b) - input(a));
  return (
    output(a) +
    (output(b) - output(a)) *
      (inverse ? fraction : Math.max(0, Math.min(1, fraction)))
  );
}
function makeNote(
  token: ScoreToken,
  clef: "treble" | "bass",
  ppq: number,
  stem?: number,
  fingeringMode: FingeringMode = "medium",
) {
  const note = new StaveNote({
    clef,
    autoStem: stem === undefined,
    stemDirection: stem,
    keys: token.pitches.length
      ? token.pitches.map(
          (p) =>
            `${p.spelling.letter}${p.spelling.accidental === "n" ? "" : (p.spelling.accidental ?? "")}/${p.spelling.octave}`,
        )
      : [
          clef === "treble"
            ? stem === 1
              ? "d/5"
              : stem === -1
                ? "g/4"
                : "b/4"
            : stem === 1
              ? "f/3"
              : stem === -1
                ? "b/2"
                : "d/3",
        ],
    duration: token.duration + (token.pitches.length ? "" : "r"),
    dots: token.dots,
  });
  note.setDuration(new Fraction(token.endTick - token.startTick, ppq * 4));
  token.pitches.forEach((p, i) => {
    if (p.accidental) note.addModifier(new Accidental(p.accidental), i);
    if (showFingering(p.fingering, fingeringMode, token.tiePrevious))
      note.addModifier(new ScoreFinger(String(p.fingering!.finger))
        .setPosition(p.fingering!.hand === "right" ? Modifier.Position.ABOVE : Modifier.Position.BELOW), i);
  });
  for (let i = 0; i < token.dots; i++)
    Dot.buildAndAttach([note], { all: true });
  const colors = {
    target: "#202124",
    hit: "#28765d",
    miss: "#c14e3b",
    muted: "#9ca3aa",
  };
  const ink =
    colors[token.muted ? "muted" : (token.pitches[0]?.highlight ?? "target")];
  note.setStyle({ fillStyle: ink, strokeStyle: ink });
  token.pitches.forEach((p, i) => {
    const ink = colors[p.highlight ?? "target"];
    note.setKeyStyle(i, { fillStyle: ink, strokeStyle: ink });
  });
  return note;
}
export function EngravedScore({
  notes,
  mode = "grand",
  song,
  fromTick,
  toTick,
  sampleRate,
  clockFrame,
  pixelsPerSecond = 110,
  running = true,
  label = "Song practice notation",
  scrub,
  selection,
  fingeringMode = "medium",
  editingFingering = false,
  onFingeringNote,
}: Omit<ScrollingStaffProps, "mode"> & {
  mode?: StaffMode;
  song: ScoreMetadata;
  fromTick: number;
  toTick: number;
  selection?: LoopSelection;
  fingeringMode?: FingeringMode;
  editingFingering?: boolean;
  onFingeringNote?: (noteIds: string[]) => void;
}) {
  const clefs = useMemo(() => staffClefs(mode), [mode]);
  const hasFingerings = notes.some(n => n.fingering?.finger != null);
  const staffY = useMemo(
    () => ({ treble: hasFingerings ? 95 : 30, bass: mode === "grand" ? (hasFingerings ? 310 : 180) : (hasFingerings ? 95 : 30) }),
    [mode, hasFingerings],
  );
  const height = mode === "grand" ? (hasFingerings ? 545 : 350) : (hasFingerings ? 350 : 220);
  const viewport = useRef<HTMLDivElement>(null),
    track = useRef<HTMLDivElement>(null),
    gutter = useRef<HTMLDivElement>(null);
  const clock = useRef(clockFrame);
  clock.current = clockFrame;
  const mapping = useRef<Point[]>([]);
  const [dragFrame, setDragFrame] = useState<number | null>(null),
    [signature, setSignature] = useState({ key: "C", time: "4/4" });
  const drag = useRef<{
    pointer: number;
    x: number;
    scoreX: number;
    moved: boolean;
  } | null>(null);
  const measures = useMemo(
    () =>
      buildScore(notes, song, fromTick, toTick).map((m) => ({
        ...m,
        voices: m.voices.filter((v) => clefs.includes(v.clef)),
      })),
    [notes, song, fromTick, toTick, clefs],
  );
  const sections = useRef<
    { start: number; end: number; key: string; time: string }[]
  >([]);
  useEffect(() => {
    const host = track.current;
    if (!host) return;
    host.replaceChildren();
    const points: Point[] = [],
      ranges: typeof sections.current = [];
    let left = 0;
    for (const measure of measures) {
      const el = document.createElement("div");
      el.className = "score-measure";
      el.dataset.measure = String(measure.number);
      el.style.left = `${left}px`;
      host.append(el);
      el.dataset.startFrame = String(
        scoreFrame(song, measure.startTick, sampleRate),
      );
      el.dataset.endFrame = String(
        scoreFrame(song, measure.endTick, sampleRate),
      );
      const renderer = new Renderer(el, Renderer.Backends.SVG),
        context = renderer.getContext();
      const staves = {
        treble: new Stave(0, staffY.treble, 400),
        bass: new Stave(0, staffY.bass, 400),
      };
      for (const stave of Object.values(staves))
        stave
          .setBegBarType(Barline.type.NONE)
          .setEndBarType(Barline.type.SINGLE)
          .setContext(context)
          .setNoteStartX(22);
      const rows = measure.voices.map((v, index) => {
        const siblings = measure.voices.filter(
          (other) => other.clef === v.clef,
        );
        const voiceIndex = measure.voices
          .slice(0, index)
          .filter((other) => other.clef === v.clef).length;
        const glyphs = v.tokens.map((t) =>
          makeNote(
            t,
            v.clef,
            song.ppq,
            siblings.length > 1 ? (voiceIndex === 0 ? 1 : -1) : undefined,
            fingeringMode,
          ).setStave(staves[v.clef]),
        );
        const voice = new Voice({
          numBeats: measure.numerator,
          beatValue: measure.denominator,
        })
          .setMode(Voice.Mode.SOFT)
          .setStave(staves[v.clef])
          .addTickables(glyphs);
        const beams = Beam.generateBeams(glyphs, {
          groups: [
            new Fraction(
              measure.denominator === 8 && measure.numerator % 3 === 0 ? 3 : 1,
              measure.denominator,
            ),
          ],
          maintainStemDirections: siblings.length > 1,
        });
        return { ...v, voice, glyphs, beams };
      });
      const voices = rows.map((r) => r.voice),
        formatter = new Formatter();
      for (const clef of clefs)
        formatter.joinVoices(
          rows.filter((r) => r.clef === clef).map((r) => r.voice),
        );
      const min = formatter.preCalculateMinTotalWidth(voices);
      const onsets = new Set(
        rows.flatMap((r) => r.tokens.map((t) => t.startTick)),
      ).size;
      const width = Math.ceil(
        Math.max(230, min * 1.35 + 60, onsets * 32 + 60) *
          Math.max(0.8, pixelsPerSecond / 110),
      );
      renderer.resize(width, height);
      el.style.width = `${width}px`;
      for (const clef of clefs) staves[clef].setWidth(width).draw();
      if (mode === "grand")
        new StaveConnector(staves.treble, staves.bass)
          .setType(StaveConnector.type.SINGLE_RIGHT)
          .setContext(context)
          .draw();
      formatter.format(voices, width - 52);
      for (const tick of formatter.getTickContexts()?.array ?? []) {
        // Chord unisons need a check on each staff, never between the two staves.
        const voices = tick.getTickables();
        for (const clef of ["treble", "bass"] as const) {
          const sameStaff = voices.filter(
            (n): n is StaveNote =>
              n instanceof StaveNote && n.getStave() === staves[clef],
          );
          separateNotationVoices(sameStaff);
        }
      }
      const aligned = new Map<number, number>();
      for (const row of rows) {
        row.voice.draw(context, staves[row.clef]);
        row.beams.forEach((b) => b.setContext(context).draw());
        row.tokens.forEach((token, i) => {
          const note = row.glyphs[i];
          if (token.hiddenRest) {
            const element = note.getSVGElement();
            if (element) {
              element.style.display = "none";
              element.setAttribute("data-filler-rest", "true");
            }
          }
          note
            .getSVGElement()
            ?.setAttribute("data-start-tick", String(token.startTick));
          note
            .getSVGElement()
            ?.setAttribute(
              "data-start-frame",
              String(scoreFrame(song, token.startTick, sampleRate)),
            );
          note
            .getSVGElement()
            ?.setAttribute(
              "data-score-x",
              String(left + note.getAbsoluteX() + note.getGlyphWidth() / 2),
            );
          aligned.set(
            token.startTick,
            Math.min(
              aligned.get(token.startTick) ?? Infinity,
              note.getAbsoluteX() + note.getGlyphWidth() / 2,
            ),
          );
          if (!token.pitches.length) return;
          token.pitches.forEach((pitch, index) => {
            const visible = showFingering(pitch.fingering, fingeringMode, token.tiePrevious);
            const elements = [note.noteHeads[index]?.getSVGElement(), ...note.getModifiers()
              .filter(m => m instanceof FretHandFinger && m.getIndex() === index).map(m => m.getSVGElement())];
            for (const element of elements) {
              if (!element) continue;
              element.setAttribute("data-note-ids", JSON.stringify(pitch.noteIds));
              element.setAttribute("data-finger", visible ? String(pitch.fingering!.finger) : "");
              element.setAttribute("data-fingering-hand", pitch.fingering?.hand ?? "");
              element.setAttribute("aria-label", `${spelledLabel(pitch.spelling)}, ${pitch.fingering?.hand ?? (row.clef === "bass" ? "left" : "right")} hand, ${visible ? `finger ${pitch.fingering!.finger}` : pitch.fingering?.reason ?? "no visible fingering"}`);
              if (editingFingering) {
                element.setAttribute("tabindex", "0");
                element.setAttribute("role", "button");
                element.classList.add("fingering-edit-note");
                element.addEventListener("pointerdown", e => e.stopPropagation());
                element.addEventListener("click", e => { e.stopPropagation(); onFingeringNote?.(pitch.noteIds); });
                element.addEventListener("keydown", e => {
                  if ((e as KeyboardEvent).key === "Enter" || (e as KeyboardEvent).key === " ") {
                    e.preventDefault(); e.stopPropagation(); onFingeringNote?.(pitch.noteIds);
                  }
                });
              }
            }
          });
          const previous = row.tokens[i - 1],
            next = row.tokens[i + 1];
          const polyphonic = rows.filter((r) => r.clef === row.clef).length > 1;
          const tieDirection = polyphonic
            ? -note.getStemDirection()
            : note.getStemDirection();
          if (token.tiePrevious && (!previous || !previous.tieNext))
            new StaveTie({
              lastNote: note,
              firstIndexes: token.pitches.map((_, i) => i),
              lastIndexes: token.pitches.map((_, i) => i),
            })
              .setDirection(tieDirection)
              .setContext(context)
              .draw();
          if (token.tieNext) {
            const nextNote = next?.tiePrevious ? row.glyphs[i + 1] : undefined;
            new StaveTie({
              firstNote: note,
              lastNote: nextNote,
              firstIndexes: token.pitches.map((_, i) => i),
              lastIndexes: token.pitches.map((_, i) => i),
            })
              .setDirection(tieDirection)
              .setContext(context)
              .draw();
          }
        });
      }
      context.setFillStyle("#54585c");
      context.setFont("Georgia", 12);
      context.fillText(String(measure.number), 10, 32);
      for (const [tick, x] of [...aligned].sort((a, b) => a[0] - b[0]))
        points.push({ frame: scoreFrame(song, tick, sampleRate), x: left + x });
      points.push({
        frame: scoreFrame(song, measure.endTick, sampleRate),
        x: left + width,
      });
      ranges.push({
        start: scoreFrame(song, measure.startTick, sampleRate),
        end: scoreFrame(song, measure.endTick, sampleRate),
        key: keyNames[Math.max(0, Math.min(14, measure.sharps + 7))],
        time: `${measure.numerator}/${measure.denominator}`,
      });
      el.querySelector("svg")?.setAttribute("aria-hidden", String(!editingFingering));
      left += width;
    }
    // At a shared barline the new measure's first note replaces the previous endpoint.
    mapping.current = [
      ...new Map(points.map((p) => [p.frame, p])).values(),
    ].sort((a, b) => a.frame - b.frame);
    sections.current = ranges;
  }, [
    measures,
    fingeringMode,
    editingFingering,
    onFingeringNote,
    song,
    sampleRate,
    pixelsPerSecond,
    clefs,
    staffY,
    height,
    mode,
  ]);
  useEffect(() => {
    const el = gutter.current;
    if (!el) return;
    el.replaceChildren();
    const renderer = new Renderer(el, Renderer.Backends.SVG);
    renderer.resize(260, height);
    const context = renderer.getContext();
    const staves = clefs.map((c) =>
      new Stave(12, staffY[c], 106)
        .setBegBarType(Barline.type.NONE)
        .setEndBarType(Barline.type.NONE)
        .addClef(c)
        .addKeySignature(signature.key)
        .addTimeSignature(signature.time)
        .setContext(context),
    );
    // Size the fixed section from the engraved modifiers, including all seven
    // sharps/flats, rather than clipping longer key signatures to a fixed width.
    const width = Math.ceil(Math.max(118, ...staves.map((s) => s.getNoteStartX() + 12)));
    renderer.resize(width + 28, height);
    el.style.width = `${width + 28}px`;
    el.dataset.key = signature.key;
    el.dataset.time = signature.time;
    // Keep stationary staff lines continuous across the fade while notes vanish.
    for (const stave of staves) stave.setWidth(width + 16).draw();
    if (mode === "grand")
      new StaveConnector(staves[0], staves[1])
        .setType(StaveConnector.type.BRACE)
        .setContext(context)
        .draw();
  }, [signature, clefs, staffY, height, mode]);
  const stopped = running ? undefined : clockFrame();
  useEffect(() => {
    let raf = 0;
    const draw = () => {
      const frame = dragFrame ?? clock.current(),
        x = interpolateScore(mapping.current, frame),
        width = viewport.current?.clientWidth ?? 800;
      if (track.current)
        track.current.style.transform = `translate3d(${width / 2 - x}px,0,0)`;
      const current =
        sections.current.find((s) => frame >= s.start && frame < s.end) ??
        sections.current.at(-1);
      if (current)
        setSignature((old) =>
          old.key === current.key && old.time === current.time
            ? old
            : { key: current.key, time: current.time },
        );
    };
    const animate = () => {
      draw();
      if (running) raf = requestAnimationFrame(animate);
    };
    animate();
    const resize = new ResizeObserver(draw);
    if (viewport.current) resize.observe(viewport.current);
    return () => {
      cancelAnimationFrame(raf);
      resize.disconnect();
    };
  }, [measures, pixelsPerSecond, running, stopped, dragFrame]);
  const preview = (clientX: number) => {
    const d = drag.current;
    if (!d) return 0;
    return Math.max(
      0,
      Math.min(
        scrub?.durationFrames ?? Infinity,
        interpolateScore(mapping.current, d.scoreX - (clientX - d.x), true),
      ),
    );
  };
  return (
    <div
      ref={viewport}
      style={{ height }}
      className={`scrolling-staff ${mode} paper-staff engraved-score${scrub ? " draggable-staff" : ""}${dragFrame !== null ? " dragging-staff" : ""}`}
      role={selection || editingFingering ? "group" : "img"}
      aria-label={`${label}, ${mode === "grand" ? "joined treble and bass staves" : `${mode} staff`}`}
      onPointerDown={(e) => {
        if (!scrub || e.button !== 0) return;
        e.preventDefault();
        e.currentTarget.setPointerCapture(e.pointerId);
        drag.current = {
          pointer: e.pointerId,
          x: e.clientX,
          scoreX: interpolateScore(mapping.current, clockFrame()),
          moved: false,
        };
      }}
      onPointerMove={(e) => {
        const d = drag.current;
        if (!scrub || !d || d.pointer !== e.pointerId) return;
        if (!d.moved && Math.abs(e.clientX - d.x) < 4) return;
        if (!d.moved) {
          d.moved = true;
          scrub.start();
        }
        setDragFrame(preview(e.clientX));
      }}
      onPointerUp={(e) => {
        const d = drag.current;
        if (!scrub || !d) return;
        const frame = preview(e.clientX);
        drag.current = null;
        e.currentTarget.releasePointerCapture(e.pointerId);
        if (d.moved) void scrub.seek(frame).finally(() => setDragFrame(null));
      }}
      onPointerCancel={() => {
        drag.current = null;
        setDragFrame(null);
      }}
      onLostPointerCapture={() => {
        if (drag.current) {
          drag.current = null;
          setDragFrame(null);
        }
      }}
    >
      <svg
        className="score-lines"
        width="100%"
        height={height}
        aria-hidden="true"
      >
        {clefs
          .map((c) => staffY[c] + 40)
          .flatMap((y) =>
            Array.from({ length: 5 }, (_, i) => (
              <line
                key={y + i}
                x1="0"
                x2="100%"
                y1={y + i * 10}
                y2={y + i * 10}
                stroke="#777"
                strokeWidth="1"
              />
            )),
          )}
      </svg>
      <div className="staff-track" ref={track} />
      <div className="score-gutter" ref={gutter} />
      <div className="staff-playhead" />
      {selection && <LoopMarkers selection={selection} label="Score selection"
        position={tick => {
          const frame = scoreFrame(song, tick, sampleRate);
          const points = mapping.current;
          if (!points.length || frame < points[0].frame) return -1;
          if (frame > points.at(-1)!.frame) return (viewport.current?.clientWidth ?? 0) + 1;
          const shift = track.current ? new DOMMatrixReadOnly(getComputedStyle(track.current).transform).m41 : 0;
          return interpolateScore(points, frame) + shift;
        }}
        tickAt={clientX => {
          const shift = track.current ? new DOMMatrixReadOnly(getComputedStyle(track.current).transform).m41 : 0;
          const x = clientX - (viewport.current?.getBoundingClientRect().left ?? 0) - shift;
          return scoreTick(song, interpolateScore(mapping.current, x, true), sampleRate);
        }} />}
    </div>
  );
}
