import { useEffect, useMemo, useRef, useState } from "react";
import { useNative } from "./native";
import { ScrollingStaff } from "./ScrollingStaff";
import type { ChordAnswer, TimelineNote } from "./types";
import "./earFlash.css";
const intervals = [
  "m2",
  "M2",
  "m3",
  "M3",
  "P4",
  "TT",
  "P5",
  "m6",
  "M6",
  "m7",
  "M7",
  "P8",
];
const names: Record<string, string> = {
  m2: "Minor 2nd",
  M2: "Major 2nd",
  m3: "Minor 3rd",
  M3: "Major 3rd",
  P4: "Perfect 4th",
  TT: "Tritone",
  P5: "Perfect 5th",
  m6: "Minor 6th",
  M6: "Major 6th",
  m7: "Minor 7th",
  M7: "Major 7th",
  P8: "Octave",
};
const keys = ["C", "G", "D", "A", "E", "B", "F", "Bb", "Eb", "Ab", "Db", "Gb"],
  letters = ["C", "D", "E", "F", "G", "A", "B"];
const empty: TimelineNote[] = [];
function NumberField({
  label,
  value,
  min,
  max,
  commit,
}: {
  label: string;
  value: number;
  min: number;
  max: number;
  commit: (n: number) => void;
}) {
  const [draft, setDraft] = useState(String(value)),
    focused = useRef(false);
  useEffect(() => {
    if (!focused.current) setDraft(String(value));
  }, [value]);
  return (
    <label>
      {label}
      <input
        aria-label={label}
        type="number"
        min={min}
        max={max}
        value={draft}
        onFocus={() => {
          focused.current = true;
        }}
        onChange={(e) => setDraft(e.target.value)}
        onBlur={() => {
          focused.current = false;
          const n = Number(draft);
          if (draft.trim() && Number.isInteger(n) && n >= min && n <= max)
            commit(n);
          else setDraft(String(value));
        }}
        onKeyDown={(e) => {
          if (e.key === "Enter") e.currentTarget.blur();
        }}
      />
    </label>
  );
}
function SelectField({
  label,
  value,
  options,
  commit,
}: {
  label: string;
  value: string;
  options: [string, string][];
  commit: (v: string) => void;
}) {
  return (
    <label>
      {label}
      <select
        aria-label={label}
        value={value}
        onChange={(e) => commit(e.target.value)}
      >
        {options
          .filter(
            ([value], index) =>
              options.findIndex((option) => option[0] === value) === index,
          )
          .map(([value, text]) => (
            <option key={value} value={value}>
              {text}
            </option>
          ))}
      </select>
    </label>
  );
}
function Options({
  label,
  values,
  selected,
  toggle,
}: {
  label: string;
  values: (string | number)[];
  selected: (string | number)[];
  toggle: (value: string | number, on: boolean) => void;
}) {
  return (
    <fieldset>
      <legend>{label}</legend>
      <div className="exercise-options">
        {values.map((value) => (
          <label className="check" key={value}>
            <input
              type="checkbox"
              aria-label={`${label} ${value}`}
              checked={selected.includes(value)}
              onChange={(e) => toggle(value, e.target.checked)}
            />
            {names[String(value)] ?? value}
          </label>
        ))}
      </div>
    </fieldset>
  );
}
function Stats({
  hits,
  misses,
  accuracy,
  feedback,
  reset,
}: {
  hits: number;
  misses: number;
  accuracy: number;
  feedback: string;
  reset: () => void;
}) {
  return (
    <div className="exercise-stats">
      <span>
        <strong>{hits}</strong> correct
      </span>
      <span>
        <strong>{misses}</strong> attempts missed
      </span>
      <span>
        <strong>{accuracy.toFixed(0)}%</strong> accuracy
      </span>
      <span role="status">
        {feedback || "Take your time. Listen, then answer."}
      </span>
      <button onClick={reset}>Reset statistics</button>
    </div>
  );
}
export function EarTraining() {
  const native = useNative(),
    p = native.state?.ear,
    c = p?.config;
  const [error, setError] = useState("");
  const notes = useMemo(() => p?.notes ?? empty, [p?.revision]);
  const cmd = (action: string, params: Record<string, unknown> = {}) => {
    setError("");
    void native
      .command("ear." + action, params)
      .catch((e) => setError(String(e.message)));
  };
  const configure = (patch: Record<string, unknown>) => cmd("configure", patch);
  const toggle = (field: string, value: string | number, enabled: boolean) =>
    cmd("option", { field, value, enabled });
  if (!p || !c) return <p>Connecting to native exercises…</p>;
  return (
    <section className="ear-flash" aria-label="Ear training exercises">
      <div className="exercise-top">
        <div>
          <span className="eyebrow">LISTEN / REMEMBER / PLAY</span>
          <h2>
            {c.mode === "interval" ? "Learn intervals" : "Play back melodies"}
          </h2>
          <p className="helper">
            Examples use the native sampled piano. Your live keyboard and
            metronome stay available.
          </p>
        </div>
        <SelectField
          label="Exercise"
          value={c.mode}
          options={[
            ["interval", "Learn Intervals"],
            ["melody", "Play Back Melodies"],
          ]}
          commit={(mode) => configure({ mode })}
        />
      </div>
      <details className="exercise-settings" open>
        <summary>Exercise settings</summary>
        <div className="form-grid">
          <NumberField
            label="Example tempo"
            value={c.tempo}
            min={20}
            max={300}
            commit={(tempo) => configure({ tempo })}
          />
          <NumberField
            label="Root MIDI note"
            value={c.root}
            min={36}
            max={84}
            commit={(root) => configure({ root })}
          />
          <NumberField
            label="Transpose semitones"
            value={c.transpose}
            min={-12}
            max={12}
            commit={(transpose) => configure({ transpose })}
          />
          <label className="check">
            <input
              type="checkbox"
              checked={c.randomRoot}
              onChange={(e) => configure({ randomRoot: e.target.checked })}
            />
            Random roots
          </label>
          {c.mode === "interval" ? (
            <>
              <SelectField
                label="Interval sequence"
                value={c.order}
                options={[
                  ["ordered", "Ordered"],
                  ["random", "Random"],
                ]}
                commit={(order) => configure({ order })}
              />
              <SelectField
                label="Example order"
                value={c.playOrder}
                options={[
                  ["forward", "Root then interval"],
                  ["reverse", "Reverse"],
                  ["harmonic", "Harmonic together"],
                ]}
                commit={(playOrder) => configure({ playOrder })}
              />
            </>
          ) : (
            <>
              <NumberField
                label="Melody groups"
                value={c.groups}
                min={2}
                max={8}
                commit={(groups) => configure({ groups })}
              />
              <NumberField
                label="Simultaneous pitches"
                value={c.pitches}
                min={1}
                max={4}
                commit={(pitches) => configure({ pitches })}
              />
              <SelectField
                label="Melody direction"
                value={c.melodyDirection}
                options={[
                  ["ascending", "Ascending"],
                  ["descending", "Descending"],
                  ["any", "Any direction"],
                ]}
                commit={(melodyDirection) => configure({ melodyDirection })}
              />
              <SelectField
                label="Range preset"
                value={`${c.range.min}-${c.range.max}`}
                options={[
                  ["48-84", "Wide · C3–C6"],
                  ["60-72", "Middle · C4–C5"],
                  ["36-60", "Bass · C2–C4"],
                  ["60-84", "Treble · C4–C6"],
                  [
                    `${c.range.min}-${c.range.max}`,
                    `Current · MIDI ${c.range.min}–${c.range.max}`,
                  ],
                ]}
                commit={(value) => {
                  const [min, max] = value.split("-").map(Number);
                  configure({ range: { min, max } });
                }}
              />
              <NumberField
                label="Range minimum MIDI"
                value={c.range.min}
                min={21}
                max={108}
                commit={(min) => configure({ range: { min } })}
              />
              <NumberField
                label="Range maximum MIDI"
                value={c.range.max}
                min={21}
                max={108}
                commit={(max) => configure({ range: { max } })}
              />
              <label className="check">
                <input
                  type="checkbox"
                  checked={c.continuous}
                  onChange={(e) => configure({ continuous: e.target.checked })}
                />
                Continuous · new melody after correct playback
              </label>
              <label className="check">
                <input
                  type="checkbox"
                  checked={c.pitchWheel}
                  onChange={(e) => configure({ pitchWheel: e.target.checked })}
                />
                Pitch-wheel shortcuts · down replay / up new
              </label>
            </>
          )}
        </div>
        {c.mode === "interval" && (
          <>
            <Options
              label="Interval qualities"
              values={intervals}
              selected={c.intervals}
              toggle={(v, on) => toggle("intervals", v, on)}
            />
            <Options
              label="Directions"
              values={["up", "down"]}
              selected={c.directions}
              toggle={(v, on) => toggle("directions", v, on)}
            />
          </>
        )}
      </details>
      <div className="exercise-actions">
        {c.mode === "interval" ? (
          <>
            <button onClick={() => cmd("root")}>Play root</button>
            <button onClick={() => cmd("interval")}>Play interval</button>
            <button onClick={() => cmd("motif")}>Play reference motif</button>
            <button className="primary" onClick={() => cmd("play")}>
              Play interval pair
            </button>
            <button onClick={() => cmd("next")}>Next interval</button>
            <button onClick={() => cmd("random")}>Random interval</button>
            <label className="check">
              <input
                type="checkbox"
                checked={p.autoplay}
                onChange={(e) => cmd("autoplay", { enabled: e.target.checked })}
              />
              Autoplay random roots
            </label>
          </>
        ) : (
          <>
            <button className="primary" onClick={() => cmd("replay")}>
              Replay melody
            </button>
            <button onClick={() => cmd("melody")}>New melody</button>
          </>
        )}
        <button
          disabled={!p.playing && !p.paused}
          onClick={() => cmd(p.paused ? "play" : "pause")}
        >
          {p.paused ? "Resume example" : "Pause example"}
        </button>
        <button onClick={() => cmd("stop")}>Stop example</button>
        <button onClick={() => cmd("skip")}>Skip example</button>
      </div>
      {p.target && (
        <>
          <p className="exercise-prompt">
            {c.mode === "interval"
              ? `Root MIDI ${p.target.root} · ${p.target.direction === "down" ? "descending" : "ascending"}`
              : `${p.target.key} major · ${c.groups} groups · play group ${Math.min(p.index + 1, c.groups)} of ${c.groups}`}
          </p>
          <p className="helper">
            {p.answerLocked
              ? "Listen first — answers are disabled during this example. Your live notes remain audible."
              : c.mode === "melody"
                ? "Play each group on MIDI or the computer piano. Fresh pressed notes form each chord within 150 ms."
                : "Choose the interval you hear."}
          </p>
          <ScrollingStaff
            mode="grand"
            notes={notes}
            sampleRate={native.state?.sampleRate ?? 48000}
            clockFrame={() =>
              p.playing
                ? native.frame()
                : p.paused
                  ? p.pauseFrame
                  : Math.max(0, (notes[0]?.startFrame ?? 0) - 12000)
            }
            running={p.playing}
            label="Ear example notation"
          />
          {c.mode === "interval" && (
            <div className="exercise-answers">
              {c.intervals.map((interval) => (
                <button
                  key={interval}
                  disabled={p.answerLocked || p.answered}
                  onClick={() => cmd("answer", { interval })}
                >
                  {names[interval]}
                </button>
              ))}
            </div>
          )}
        </>
      )}
      {(error || p.error) && (
        <p role="alert" className="exercise-error">
          {error || p.error}
        </p>
      )}
      <Stats {...p} reset={() => cmd("reset")} />
      {c.mode === "interval" && (
        <p className="helper">
          Reference motifs are original four-note root/interval phrases,
          released under CC0. The pair plays two groups; harmonic plays both
          notes together. Autoplay changes the root each cycle.
        </p>
      )}
    </section>
  );
}
export function FlashCards() {
  const native = useNative(),
    p = native.state?.flash,
    c = p?.config;
  const [error, setError] = useState(""),
    [answer, setAnswer] = useState<ChordAnswer>({
      root: "C",
      accidental: "natural",
      quality: "major",
    });
  const cmd = (action: string, params: Record<string, unknown> = {}) => {
    setError("");
    void native
      .command("flash." + action, params)
      .catch((e) => setError(String(e.message)));
  };
  const configure = (patch: Record<string, unknown>) => cmd("configure", patch);
  const notes = useMemo(() => p?.target?.notes ?? empty, [p?.revision]);
  useEffect(() => {
    const handler = (e: KeyboardEvent) => {
      if (
        e.repeat ||
        e.ctrlKey ||
        e.metaKey ||
        e.altKey ||
        (e.target as HTMLElement).closest(
          'input,textarea,select,[contenteditable="true"]',
        ) ||
        c?.mode !== "noteMath" ||
        p?.answered ||
        p?.answerLocked
      )
        return;
      const index = Number(e.key) - 1;
      if (index >= 0 && index < 7) {
        e.preventDefault();
        cmd("answer", { answer: letters[index] });
      }
    };
    window.addEventListener("keydown", handler);
    return () => window.removeEventListener("keydown", handler);
  }, [native, p?.answered, p?.answerLocked, c?.mode]);
  if (!p || !c) return <p>Connecting to native cards…</p>;
  return (
    <section className="ear-flash" aria-label="Flash card exercises">
      <div className="exercise-top">
        <div>
          <span className="eyebrow">NAME / COUNT / RECOGNIZE</span>
          <h2>
            {c.mode === "noteMath" ? "Note Math" : "Chord identification"}
          </h2>
          <p className="helper">
            Count diatonic letters, or recognize the spelled chord voicing.
          </p>
        </div>
        <SelectField
          label="Card type"
          value={c.mode}
          options={[
            ["noteMath", "Note Math"],
            ["chord", "Chord Identification"],
          ]}
          commit={(mode) => configure({ mode })}
        />
      </div>
      <details className="exercise-settings" open>
        <summary>Card settings</summary>
        {c.mode === "noteMath" ? (
          <>
            <Options
              label="Note roots"
              values={letters}
              selected={c.roots}
              toggle={(value, enabled) =>
                cmd("option", { field: "roots", value, enabled })
              }
            />
            <Options
              label="Diatonic distances"
              values={[1, 2, 3, 4, 5, 6, 7]}
              selected={c.distances}
              toggle={(value, enabled) =>
                cmd("option", { field: "distances", value, enabled })
              }
            />
          </>
        ) : (
          <>
            <SelectField
              label="Chord staff"
              value={c.staff}
              options={[
                ["treble", "Treble"],
                ["bass", "Bass"],
                ["grand", "Grand"],
              ]}
              commit={(staff) => configure({ staff })}
            />
            <Options
              label="Chord keys"
              values={keys}
              selected={c.keys}
              toggle={(value, enabled) =>
                cmd("option", { field: "keys", value, enabled })
              }
            />
            <Options
              label="Chord inversions"
              values={[0, 1, 2]}
              selected={c.inversions}
              toggle={(value, enabled) =>
                cmd("option", { field: "inversions", value, enabled })
              }
            />
          </>
        )}
      </details>
      <div className="exercise-actions">
        <button className="primary" onClick={() => cmd("next")}>
          Next card
        </button>
      </div>
      {p.target ? (
        <>
          {c.mode === "noteMath" ? (
            <>
              <div className="note-math-question">
                {p.target.distance}
                {p.target.distance === 1
                  ? "st"
                  : p.target.distance === 2
                    ? "nd"
                    : p.target.distance === 3
                      ? "rd"
                      : "th"}{" "}
                of {p.target.root}
              </div>
              <p className="helper">
                Count the root as 1. Use keys 1–7 for C D E F G A B.
              </p>
              <div className="exercise-answers">
                {letters.map((answer, i) => (
                  <button
                    disabled={p.answered || p.answerLocked}
                    key={answer}
                    onClick={() => cmd("answer", { answer })}
                  >
                    <kbd>{i + 1}</kbd>
                    {answer}
                  </button>
                ))}
              </div>
            </>
          ) : (
            <>
              <p className="exercise-prompt">
                Key: {p.target.key} major · identify root, accidental and
                quality
              </p>
              <ScrollingStaff
                mode={c.staff}
                notes={notes}
                sampleRate={native.state?.sampleRate ?? 48000}
                clockFrame={() => 0}
                running={false}
                label="Chord identification notation"
              />
              <div className="form-grid">
                <SelectField
                  label="Chord root answer"
                  value={answer.root}
                  options={letters.map((x) => [x, x])}
                  commit={(root) => setAnswer({ ...answer, root })}
                />
                <SelectField
                  label="Chord accidental answer"
                  value={answer.accidental}
                  options={[
                    ["natural", "Natural"],
                    ["flat", "Flat"],
                    ["sharp", "Sharp"],
                  ]}
                  commit={(accidental) => setAnswer({ ...answer, accidental })}
                />
                <SelectField
                  label="Chord quality answer"
                  value={answer.quality}
                  options={[
                    ["major", "Major"],
                    ["minor", "Minor"],
                    ["diminished", "Diminished"],
                  ]}
                  commit={(quality) => setAnswer({ ...answer, quality })}
                />
              </div>
              <button
                className="primary"
                disabled={p.answered || p.answerLocked}
                onClick={() => cmd("answer", { answer })}
              >
                Check chord answer
              </button>
            </>
          )}
        </>
      ) : (
        <p className="exercise-prompt">Choose Next card to begin.</p>
      )}
      {error && (
        <p role="alert" className="exercise-error">
          {error}
        </p>
      )}
      {p.answerLocked && (
        <p className="helper">
          Answers are disabled while the native Ear example plays or is paused.
          Your live notes remain audible.
        </p>
      )}
      <Stats {...p} reset={() => cmd("reset")} />
    </section>
  );
}
