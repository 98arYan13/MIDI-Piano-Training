import { useEffect, useMemo, useRef, useState } from "react";
import { useNative } from "./native";
import {
  computerMap,
  keyboardWidth,
  pianoKeys,
  VirtualInput,
} from "./pianoModel";
import "./practice.css";

export function PianoKeyboard() {
  const native = useNative();
  const input = useMemo(
    () =>
      new VirtualInput((type, params) => {
        void native.command(type, params).catch(() => {});
      }, native.source),
    [native],
  );
  const [hidden, setHidden] = useState(false),
    [focused, setFocused] = useState(false),
    [labels, setLabels] = useState(true),
    [velocity, setVelocity] = useState(90),
    [octave, setOctave] = useState(4),
    [pedal, setPedal] = useState(false),
    [width, setWidth] = useState(1040);
  const scroll = useRef<HTMLDivElement>(null);
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
  }, [hidden]);
  const keys = pianoKeys().filter(
    (k) =>
      !focused ||
      (k.midi >= Math.max(21, 12 * (octave + 1) - 12) &&
        k.midi <= Math.min(108, 12 * (octave + 1) + 24)),
  );
  const firstLeft = keys[0]?.left ?? 0,
    whites = keys.filter((k) => !k.black).length;
  const span = focused ? Math.max(width, whites * 30) : keyboardWidth(width),
    unit = span / whites;
  const moveOctave = (delta: number) => {
    const next = Math.max(1, Math.min(7, octave + delta));
    setOctave(next);
    if (!focused && scroll.current) {
      const key = pianoKeys().find((k) => k.midi === 12 * (next + 1));
      scroll.current.scrollTo({
        left: Math.max(0, (key?.left ?? 0) * unit - width / 2),
      });
    }
  };
  const ready = native.connected && native.state?.pianoReady;
  return (
    <section className="piano-panel" aria-label="Global piano keyboard">
      <div className="piano-heading">
        <div>
          <span className="eyebrow">ALWAYS WITH YOU</span>
          <h2>Piano keyboard</h2>
        </div>
        <button onClick={() => setHidden(!hidden)} aria-expanded={!hidden}>
          {hidden ? "Show keyboard" : "Hide keyboard"}
        </button>
      </div>
      {!hidden && (
        <>
          <div className="piano-tools">
            <button aria-pressed={!focused} onClick={() => setFocused(false)}>
              Full · 88 keys
            </button>
            <button aria-pressed={focused} onClick={() => setFocused(true)}>
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
                    onChange={(e) => setVelocity(Number(e.target.value))}
                  />
                </label>
                <label>
                  <input
                    type="checkbox"
                    checked={labels}
                    onChange={(e) => setLabels(e.target.checked)}
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
                return (
                  <button
                    key={k.midi}
                    data-midi={k.midi}
                    data-buses={buses.join(" ")}
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
                    {buses.length > 1 && <i className="mixed-source" />}
                  </button>
                );
              })}
            </div>
          </div>
          <div className="piano-legend">
            <span className="live-dot">Live / MIDI</span>
            <span className="playback-dot">Playback</span>
            <span className="example-dot">Example</span>
            <span>
              {native.state?.heldNotes.some((n) => n.sustained)
                ? "Native sustain active"
                : "A0–C8 · C4 = MIDI 60"}
            </span>
            <span>Drag the horizontal scrollbar to explore all octaves</span>
          </div>
        </>
      )}
    </section>
  );
}
