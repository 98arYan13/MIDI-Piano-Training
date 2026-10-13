import { useEffect, useRef, useState } from "react";

export type LoopSelection = {
  a: number;
  b: number;
  duration: number;
  enabled: boolean;
  start: () => Promise<void>;
  change: (a: number, b: number) => Promise<void>;
};
// Both score and overview markers commit once on release, on the native transport.
export function LoopMarkers({ selection, position, tickAt, label = "Selection" }: {
  selection: LoopSelection;
  position: (tick: number) => number;
  tickAt: (clientX: number) => number;
  label?: string;
}) {
  const host = useRef<HTMLDivElement>(null);
  const [preview, setPreview] = useState<{ edge: "a" | "b"; tick: number } | null>(null);
  const drag = useRef<{ edge: "a" | "b"; pointer: number; ready: Promise<void> } | null>(null);
  const a = preview?.edge === "a" ? preview.tick : selection.a;
  const b = preview?.edge === "b" ? preview.tick : selection.b;
  useEffect(() => {
    let raf = 0;
    const draw = () => {
      const el = host.current;
      if (el) {
        const start = position(a), end = position(b);
        const width = el.clientWidth;
        const shade = el.querySelector<HTMLElement>(".loop-shade")!;
        shade.style.left = `${Math.max(0, start)}px`;
        shade.style.width = `${Math.max(0, Math.min(width, end) - Math.max(0, start))}px`;
        for (const [index, x] of [start, end].entries()) {
          const marker = el.querySelectorAll<HTMLElement>(".loop-marker")[index];
          marker.style.left = `${x}px`;
          marker.hidden = x < 0 || x > width;
        }
      }
      raf = requestAnimationFrame(draw);
    };
    draw();
    return () => cancelAnimationFrame(raf);
  }, [a, b, position]);
  const clamp = (edge: "a" | "b", tick: number) => Math.round(
    edge === "a" ? Math.max(0, Math.min(selection.b - 1, tick))
      : Math.max(selection.a + 1, Math.min(selection.duration, tick)),
  );
  const commit = async (edge: "a" | "b", tick: number, ready: Promise<void>) => {
    try {
      await ready;
      await selection.change(edge === "a" ? tick : selection.a, edge === "b" ? tick : selection.b);
    } finally { setPreview(null); }
  };
  return <div ref={host} className={`loop-markers${selection.enabled ? " enabled" : ""}`}>
    <div className="loop-shade" />
    {(["a", "b"] as const).map(edge => <button key={edge}
      className={`loop-marker loop-${edge}`} role="slider"
      aria-label={`${label} ${edge === "a" ? "start" : "end"}`}
      aria-valuemin={edge === "a" ? 0 : a + 1}
      aria-valuemax={edge === "a" ? b - 1 : selection.duration}
      aria-valuenow={edge === "a" ? a : b}
      title={`Drag ${edge === "a" ? "Start" : "End"}; arrow keys fine-tune`}
      onPointerDown={e => {
        if (e.button !== 0) return;
        e.stopPropagation(); e.preventDefault();
        e.currentTarget.setPointerCapture(e.pointerId);
        drag.current = { edge, pointer: e.pointerId, ready: selection.start() };
      }}
      onPointerMove={e => {
        e.stopPropagation();
        if (drag.current?.pointer === e.pointerId)
          setPreview({ edge, tick: clamp(edge, tickAt(e.clientX)) });
      }}
      onPointerUp={e => {
        e.stopPropagation();
        const d = drag.current;
        if (!d || d.pointer !== e.pointerId) return;
        const tick = clamp(edge, tickAt(e.clientX));
        drag.current = null;
        e.currentTarget.releasePointerCapture(e.pointerId);
        setPreview({ edge, tick });
        void commit(edge, tick, d.ready);
      }}
      onPointerCancel={e => { e.stopPropagation(); drag.current = null; setPreview(null); }}
      onLostPointerCapture={() => { if (drag.current) { drag.current = null; setPreview(null); } }}
      onKeyDown={e => {
        if (!["ArrowLeft", "ArrowRight", "Home", "End"].includes(e.key)) return;
        e.preventDefault(); e.stopPropagation();
        const value = edge === "a" ? a : b;
        const tick = clamp(edge, e.key === "Home" ? 0 : e.key === "End" ? selection.duration
          : value + (e.key === "ArrowLeft" ? -1 : 1) * (e.shiftKey ? 16 : 1));
        void commit(edge, tick, selection.start());
      }}><span>{edge === "a" ? "Start" : "End"}</span></button>)}
  </div>;
}
