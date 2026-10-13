import { useEffect, useMemo, useRef, useState } from "react";
import { useNative } from "./native";
import {
  computerMap,
  keyboardWidth,
  keyboardSizes,
  fittingKeyboardSize,
  pianoKeys,
  VirtualInput,
} from "./pianoModel";
import "./practice.css";
import type { NativeState } from "./types";

export function PianoKeyboard({ hintContext, focusMode = false }: { hintContext?: "play" | "staff" | "ear"; focusMode?: boolean }) {
  const native = useNative();
  const [compact, setCompact] = useState(() => matchMedia("(max-width: 1100px)").matches);
  const [toolsOpen, setToolsOpen] = useState(false);
  useEffect(() => {
    const query = matchMedia("(max-width: 1100px)");
    const change = () => setCompact(query.matches);
    query.addEventListener("change", change);
    return () => query.removeEventListener("change", change);
  }, []);
  const hinted = hintContext === "play" ? native.state?.transport
    : hintContext === "staff" ? native.state?.practice
    : hintContext === "ear" ? native.state?.ear : undefined;
  const input = useMemo(
    () =>
      new VirtualInput((type, params) => {
        void native.command(type, params).catch(() => {});
      }, native.source),
    [native],
  );
  const savedKeyboard = native.state?.preferences.keyboard;
  const [keyboard, setKeyboard] = useState(savedKeyboard ?? { hidden: false, focused: false, size: 88, labels: true, velocity: 90, octave: 4 });
  const pendingChanges = useRef(0);
  const appliedRevision = useRef(0);
  const generation = useRef(native.state?.generation);
  useEffect(() => {
    if (generation.current !== native.state?.generation) {
      generation.current = native.state?.generation;
      appliedRevision.current = 0;
    }
    if (savedKeyboard && !pendingChanges.current && (native.state?.preferencesRevision ?? 0) >= appliedRevision.current)
      setKeyboard(savedKeyboard);
  }, [native.state?.generation, native.state?.preferencesRevision]);
  const { hidden, focused, size, labels, velocity, octave } = keyboard;
  const configure = (patch: Partial<typeof keyboard>) => {
    setKeyboard(current => ({ ...current, ...patch }));
    ++pendingChanges.current;
    void (async () => {
      let acknowledged: NativeState | undefined;
      try {
        acknowledged = await native.command("preferences.configure", { keyboard: patch }) as NativeState;
        appliedRevision.current = Math.max(appliedRevision.current, acknowledged.preferencesRevision);
      } catch { /* NativeClient exposes the save error. */ }
      finally {
        --pendingChanges.current;
        if (!pendingChanges.current) {
          const current = native.state;
          const latest = acknowledged && acknowledged.preferencesRevision >= (current?.preferencesRevision ?? 0) ? acknowledged : current;
          if (latest) setKeyboard(latest.preferences.keyboard);
        }
      }
    })();
  };
  const [pedal, setPedal] = useState(false),
    [width, setWidth] = useState(1040);
  const scroll = useRef<HTMLDivElement>(null);
  const lastAttack = useRef({ generation: 0, sequence: 0 });
  useEffect(() => {
    const generation = native.state?.generation ?? 0;
    if (lastAttack.current.generation !== generation)
      lastAttack.current = { generation, sequence: 0 };
    for (const event of native.events) {
      if (event.sequence <= lastAttack.current.sequence) continue;
      lastAttack.current.sequence = event.sequence;
      if (event.type !== "note" || event.event.kind !== 0 || event.event.bus === 3)
        continue;
      const key = scroll.current?.querySelector<HTMLElement>(
        `[data-midi="${event.event.note}"]`,
      );
      if (!key) continue;
      // Retrigger even when sustain or another owner keeps this pitch highlighted.
      for (const animation of key.getAnimations()) animation.cancel();
      key.animate([{ filter: "brightness(1.8)" }, { filter: "brightness(1)" }], {
        duration: 100,
      });
    }
  }, [native.events, native.state?.generation]);
  const settings = useRef({ velocity, octave });
  settings.current = { velocity, octave };
  useEffect(() => {
    const typing = (target: EventTarget | null) =>
      target instanceof HTMLElement &&
      !!target.closest('input,textarea,select,[contenteditable="true"]');
    const down = (e: KeyboardEvent) => {
      if (
        e.defaultPrevented ||
        typing(e.target) ||
        e.ctrlKey ||
        e.altKey ||
        e.metaKey ||
        e.repeat
      )
        return;
      const offset = computerMap[e.code];
      if (offset !== undefined) {
        e.preventDefault();
        input.on(
          `key:${e.code}`,
          12 * (settings.current.octave + 1) + offset,
          settings.current.velocity,
        );
      }
      if (e.code === "Space") {
        e.preventDefault();
        input.pedal(true);
        setPedal(true);
      }
    };
    const up = (e: KeyboardEvent) => {
      input.off(`key:${e.code}`);
      if (e.code === "Space") {
        input.pedal(false);
        setPedal(false);
      }
    };
    const release = () => {
      input.release();
      setPedal(false);
    };
    const visibility = () => {
      if (document.hidden) release();
    };
    window.addEventListener("keydown", down);
    window.addEventListener("keyup", up);
    window.addEventListener("blur", release);
    document.addEventListener("visibilitychange", visibility);
    return () => {
      window.removeEventListener("keydown", down);
      window.removeEventListener("keyup", up);
      window.removeEventListener("blur", release);
      document.removeEventListener("visibilitychange", visibility);
      release();
    };
  }, [input]);
  useEffect(() => {
    const el = scroll.current;
    if (!el) return;
    const observer = new ResizeObserver(() => setWidth(el.clientWidth));
    observer.observe(el);
    setWidth(el.clientWidth);
    return () => observer.disconnect();
  }, [hidden, focusMode]);
  const autoSize = compact && hintContext === "play" && !focused && size === 88;
  const displaySize = autoSize ? fittingKeyboardSize(width) : size;
  const keys = pianoKeys(focused ? 88 : displaySize).filter(
    (k) =>
      !focused ||
      (k.midi >= Math.max(21, 12 * (octave + 1) - 12) &&
        k.midi <= Math.min(108, 12 * (octave + 1) + 24)),
  );
  const firstLeft = keys[0]?.left ?? 0,
    whites = keys.filter((k) => !k.black).length;
  const span =
      autoSize ? width : focused || size < 88
        ? Math.max(width, whites * 30)
        : keyboardWidth(width),
    unit = span / whites;
  const moveOctave = (delta: number) => {
    const next = Math.max(1, Math.min(7, octave + delta));
    configure({ octave: next });
    if (!focused && scroll.current) {
      const key = keys.find((k) => k.midi === 12 * (next + 1));
      scroll.current.scrollTo({
        left: Math.max(
          0,
          ((key?.left ?? firstLeft) - firstLeft) * unit - width / 2,
        ),
      });
    }
  };
  const ready = native.connected && native.state?.pianoReady;
  return (
    <section className="piano-panel" aria-label="Global piano keyboard">
      <div className="piano-heading">
        <button onClick={() => configure({ hidden: !hidden })} aria-expanded={!hidden}>
          {hidden ? "Show keyboard" : "Hide keyboard"}
        </button>
        <details className="piano-options" open={!compact || toolsOpen}
          onToggle={e => { if (compact) setToolsOpen(e.currentTarget.open); }}>
          <summary>Keyboard options</summary>
          <div className="piano-tools">
            <select
              aria-label="Keyboard size"
              value={focused ? "focused" : size}
              onChange={(e) => {
                configure({ size: Number(e.target.value), focused: false });
              }}
            >
              {focused && (
                <option value="focused" disabled>
                  Focused range
                </option>
              )}
              {keyboardSizes.map((count) => (
                <option key={count} value={count}>
                  {count === 88 ? (compact && hintContext === "play" ? `Auto · ${displaySize} keys` : "Full · 88 keys") : `${count} keys`}
                </option>
              ))}
            </select>
            <button aria-pressed={focused} onClick={() => configure({ focused: true })}>
              Focused range
            </button>
            <div className="octave-controls">
              <button
                aria-label="Previous octave"
                onClick={() => moveOctave(-1)}
              >
                −
              </button>
              <span>Computer keys · C{octave}</span>
              <button aria-label="Next octave" onClick={() => moveOctave(1)}>
                +
              </button>
            </div>
            <details>
              <summary>Keyboard settings</summary>
              <div className="keyboard-settings">
                <label>
                  Velocity <output>{velocity}</output>
                  <input
                    aria-label="Virtual key velocity"
                    type="range"
                    min="1"
                    max="127"
                    value={velocity}
                    onChange={(e) => configure({ velocity: Number(e.target.value) })}
                  />
                </label>
                <label>
                  <input
                    type="checkbox"
                    checked={labels}
                    onChange={(e) => configure({ labels: e.target.checked })}
                  />
                  Note labels
                </label>
                <p>
                  A W S E D F T G Y H U J K O L P ; · Space: sustain · M:
                  metronome
                </p>
              </div>
            </details>
            <button
              aria-pressed={pedal}
              disabled={!ready}
              onClick={() => {
                input.pedal(!pedal);
                setPedal(!pedal);
              }}
            >
              Sustain {pedal ? "down" : "up"}
            </button>
          </div>
        </details>
      </div>
      {(!hidden || focusMode) && (
        <>
          <div className="piano-scroll" ref={scroll}>
            <div
              className="piano-keys"
              style={{ width: span }}
              aria-label={`${keys.length} piano keys`}
            >
              {keys.map((k) => {
                const notes =
                  native.state?.heldNotes.filter(
                    (n) => n.note === k.midi && (n.held || n.sustained),
                  ) ?? [];
                const buses = [...new Set(notes.map((n) => n.bus))];
                const hint = hinted?.config.hints
                  ? hinted.hintKeys?.find(n => n.midiNote === k.midi)?.hand
                  : undefined;
                return (
                  <button
                    key={k.midi}
                    data-midi={k.midi}
                    data-buses={buses.join(" ")}
                    data-hint={hint}
                    className={`piano-key ${k.black ? "black" : "white"} ${buses.join(" ")} ${notes.some((n) => n.sustained && !n.held) ? "sustained" : ""}`}
                    style={{
                      left: (k.left - firstLeft) * unit,
                      width: unit * (k.black ? 0.64 : 1),
                    }}
                    aria-label={`${k.label}, MIDI ${k.midi}`}
                    aria-pressed={notes.some((n) => n.held)}
                    disabled={!ready}
                    onPointerDown={(e) => {
                      if (e.button !== 0) return;
                      e.preventDefault();
                      e.currentTarget.setPointerCapture(e.pointerId);
                      input.on(`pointer:${e.pointerId}`, k.midi, velocity);
                    }}
                    onPointerUp={(e) => input.off(`pointer:${e.pointerId}`)}
                    onPointerCancel={(e) => input.off(`pointer:${e.pointerId}`)}
                    onLostPointerCapture={(e) =>
                      input.off(`pointer:${e.pointerId}`)
                    }
                    onKeyDown={(e) => {
                      if ((e.key === "Enter" || e.key === " ") && !e.repeat) {
                        e.preventDefault();
                        input.on(`button:${k.midi}`, k.midi, velocity);
                      }
                    }}
                    onKeyUp={(e) => {
                      if (e.key === "Enter" || e.key === " ")
                        input.off(`button:${k.midi}`);
                    }}
                    onBlur={() => input.off(`button:${k.midi}`)}
                  >
                    {labels && <span>{k.label}</span>}
                    {hint && <i className={`key-hint hint-${hint}`} aria-hidden="true" />}
                    {buses.length > 1 && <i className="mixed-source" />}
                  </button>
                );
              })}
            </div>
          </div>
          <div className="piano-legend">
            {hinted?.config.hints && <span>Hints · teal left hand · orange right hand</span>}
            <span className="live-dot">Live / MIDI</span>
            <span className="playback-dot">Playback</span>
            <span className="example-dot">Example</span>
            <span>
              {native.state?.heldNotes.some((n) => n.sustained)
                ? "Native sustain active"
                : `${keys[0]?.label}–${keys.at(-1)?.label} · C4 = MIDI 60`}
            </span>
            <span>Drag the horizontal scrollbar to explore all octaves</span>
          </div>
        </>
      )}
    </section>
  );
}
