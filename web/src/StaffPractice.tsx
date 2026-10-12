import { pianoKeys } from "./pianoModel";
import { useEffect, useMemo, useRef, useState } from "react";
import { useNative } from "./native";
import { ScrollingStaff } from "./ScrollingStaff";
import { spelledLabel } from "./notationModel";
import { practiceAction } from "./practiceTransport";
import { PlayerControls, RestartButton } from "./PlayerControls";
import type { PracticeConfig, TimelineNote } from "./types";
const keys = [
  "C",
  "G",
  "D",
  "A",
  "E",
  "B",
  "F",
  "Bb",
  "Eb",
  "Ab",
  "Db",
  "Gb",
  "Chromatic",
];
const empty: TimelineNote[] = [];
function NumberSetting({
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
  const save = () => {
    focused.current = false;
    const n = Number(draft);
    if (draft.trim() && Number.isInteger(n) && n >= min && n <= max) commit(n);
    else setDraft(String(value));
  };
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
        onBlur={save}
        onKeyDown={(e) => {
          if (e.key === "Enter") e.currentTarget.blur();
        }}
      />
    </label>
  );
}
export function StaffPractice() {
  const native = useNative(),
    p = native.state?.practice,
    config = p?.config;
  const [error, setError] = useState("");
  const notes = useMemo(() => p?.notes ?? empty, [p?.revision]);
  const command = (type: string, params: Record<string, unknown> = {}) => {
    setError("");
    void native
      .command(type, params)
      .catch((e) => setError(String(e.message ?? e)));
  };
  const edit = (patch: Partial<PracticeConfig>) =>
    command("practice.configure", patch);
  if (!config || !p) return <p>Connecting to native Staff practice…</p>;
  const option = (patch: Partial<PracticeConfig["generatorOptions"]>) =>
    edit({
      generatorOptions: { ...patch } as PracticeConfig["generatorOptions"],
    });
  const select = (
    label: string,
    value: string,
    values: [string, string][],
    change: (v: string) => void,
  ) => (
    <label>
      {label}
      <select
        aria-label={label}
        value={value}
        onChange={(e) => change(e.target.value)}
      >
        {values.map(([v, text]) => (
          <option key={v} value={v}>
            {text}
          </option>
        ))}
      </select>
    </label>
  );
  const check = (
    label: string,
    value: boolean,
    change: (v: boolean) => void,
  ) => (
    <label className="practice-check">
      <input
        type="checkbox"
        checked={value}
        onChange={(e) => change(e.target.checked)}
      />
      {label}
    </label>
  );
  const opts = config.generatorOptions,
    symbol = config.staff === "chordSymbol",
    generator = symbol ? "chordSymbol" : config.generator;
  const current = p.targets[p.index];
  return (
    <section className="staff-practice" aria-label="Staff practice">
      <div className="staff-preview-tools">
        {select(
          "Staff view",
          config.staff,
          [
            ["treble", "Treble"],
            ["bass", "Bass"],
            ["grand", "Grand"],
            ["chordSymbol", "Chord symbols"],
          ],
          (v) => edit({ staff: v as PracticeConfig["staff"] }),
        )}
        {!symbol &&
          select(
            "Generator",
            config.generator,
            [
              ["random", "Random notes"],
              ["triads", "Triads"],
              ["sevenths", "Sevenths"],
              ["progression", "Progressions"],
              ["position", "Five-finger position"],
              ["intervals", "Intervals"],
            ],
            (v) => edit({ generator: v as PracticeConfig["generator"] }),
          )}
        {select(
          "Practice mode",
          config.mode,
          [
            ["wait", "Wait"],
            ["scroll", "Scroll"],
          ],
          (v) => edit({ mode: v as PracticeConfig["mode"] }),
        )}
        <button onClick={() => command("practice.reset")}>Reset score</button>
        <button onClick={() => command("practice.stop")}>Stop exercise</button>
        <button
          onClick={() => {
            void document.documentElement
              .requestFullscreen()
              .catch((e) => setError(e.message));
          }}
        >
          Exercise fullscreen
        </button>
      </div>
      <div className="practice-player">
        <PlayerControls
          hand={config.hand}
          onHand={(hand) =>
            edit({
              hand,
              staff:
                hand === "both" ? "grand" : hand === "left" ? "bass" : "treble",
            })
          }
          playing={p.running}
          playLabel={
            p.running
              ? "Pause exercise"
              : p.paused
                ? "Resume exercise"
                : "Start exercise"
          }
          onPlay={() => command(practiceAction(p.running, p.paused))}
        />
        <div className="practice-score">
          {symbol ? (
            <div
              className="chord-symbol-target"
              aria-label="Chord symbol target"
            >
              {current?.symbol ?? "Start an exercise"}
              <small>
                Play the displayed chord voicing:{" "}
                {current?.pitches
                  .map((n) => spelledLabel(n.spelling))
                  .join(" · ")}
              </small>
            </div>
          ) : (
            <ScrollingStaff
              mode={config.staff as "treble" | "bass" | "grand"}
              notes={notes}
              sampleRate={native.state?.sampleRate ?? 48000}
              clockFrame={() =>
                config.mode === "wait"
                  ? p.clockFrame
                  : p.running
                    ? Math.max(0, native.frame() - p.originFrame)
                    : p.clockFrame
              }
              running={p.running}
              paper
              tempo={config.tempo}
            />
          )}
          <RestartButton
            label="Restart exercise"
            onClick={() => command("practice.reset")}
          />
        </div>
        <div className="practice-progress" aria-label="Exercise progress">
          <div
            style={{
              width: `${p.targets.length ? (p.index / p.targets.length) * 100 : 0}%`,
            }}
          />
        </div>
      </div>
      <div className="practice-stats" aria-live="polite">
        <span>
          Hits <b>{p.hits}</b>
        </span>
        <span>
          Misses <b>{p.misses}</b>
        </span>
        <span>
          Accuracy <b>{p.accuracy.toFixed(1)}%</b>
        </span>
        <span>
          Elapsed <b>{Math.floor(p.elapsed)}s</b>
        </span>
        <span>
          {p.running
            ? `Target ${p.index + 1} / ${p.targets.length}`
            : p.paused
              ? `Paused · Target ${p.index + 1} / ${p.targets.length}`
              : "Exercise stopped"}
        </span>
      </div>
      {(error || p.error) && (
        <p role="alert" className="error">
          {error || p.error}
        </p>
      )}
      {generator === "position" && current && (
        <p className="helper">
          Five-finger position · play the displayed note with finger{" "}
          {current.fingering}. The first target uses your Starting finger
          setting; later notes stay in the same hand position.
        </p>
      )}
      <p className="helper">
        {config.mode === "wait"
          ? "Wait: press the target pitches together within 150 ms. Release wrong notes and retry."
          : `Scroll: native ${config.tempo} BPM, ±${config.toleranceMs} ms. Press each target at NOW.`}{" "}
        Settings edits stop only this exercise. Live piano and metronome
        continue.
      </p>
      <details className="practice-settings">
        <summary>Staff settings</summary>
        <div className="practice-settings-grid">
          {select(
            "Key",
            config.key,
            keys.map((k) => [k, k]),
            (v) => edit({ key: v }),
          )}
          {select(
            "Scale",
            opts.scale,
            [
              ["major", "Major"],
              ["naturalMinor", "Natural minor"],
              ["harmonicMinor", "Harmonic minor"],
              ["melodicMinor", "Melodic minor (ascending)"],
            ],
            (v) => option({ scale: v as typeof opts.scale }),
          )}
          <NumberSetting
            label="Minimum MIDI note"
            value={config.range.min}
            min={21}
            max={108}
            commit={(n) =>
              edit({ range: { min: n } as PracticeConfig["range"] })
            }
          />
          <NumberSetting
            label="Maximum MIDI note"
            value={config.range.max}
            min={21}
            max={108}
            commit={(n) =>
              edit({ range: { max: n } as PracticeConfig["range"] })
            }
          />
          <label>
            Smoothness {config.smoothness}
            <input
              aria-label="Smoothness"
              type="range"
              min="0"
              max="6"
              value={config.smoothness}
              onChange={(e) => edit({ smoothness: Number(e.target.value) })}
            />
          </label>
          <NumberSetting
            label="Practice tempo"
            value={config.tempo}
            min={20}
            max={300}
            commit={(n) => edit({ tempo: n })}
          />
          <label>
            Tempo slider
            <input
              aria-label="Practice tempo slider"
              type="range"
              min="20"
              max="300"
              value={config.tempo}
              onChange={(e) => edit({ tempo: Number(e.target.value) })}
            />
          </label>
          <NumberSetting
            label="Timing tolerance ms"
            value={config.toleranceMs}
            min={20}
            max={1000}
            commit={(n) => edit({ toleranceMs: n })}
          />
          <NumberSetting
            label="Generation seed"
            value={config.seed}
            min={0}
            max={2147483647}
            commit={(n) => edit({ seed: n })}
          />
          {generator !== "position" &&
            select(
              "Hands",
              String(opts.hands),
              [
                ["1", "1 hand"],
                ["2", "2 hands"],
              ],
              (v) => option({ hands: Number(v) }),
            )}
          {generator === "random" && (
            <>
              <NumberSetting
                label="Simultaneous notes"
                value={opts.notes}
                min={1}
                max={5}
                commit={(n) => option({ notes: n })}
              />
              {check("Chord based", opts.chordBased, (v) =>
                option({ chordBased: v }),
              )}
            </>
          )}
          {["triads", "sevenths", "progression", "chordSymbol"].includes(
            generator,
          ) &&
            check("Allow inversions", opts.inversions, (v) =>
              option({ inversions: v }),
            )}
          {generator === "sevenths" &&
            check("Open voicing", opts.openVoicing, (v) =>
              option({ openVoicing: v }),
            )}
          {generator === "progression" &&
            select(
              "Progression",
              opts.progression,
              [
                ["autumnLeaves", "Autumn Leaves"],
                ["fifties", "50s progression"],
                ["circle", "Circle of fifths"],
                ["substitution", "Basic substitution"],
              ],
              (v) => option({ progression: v as typeof opts.progression }),
            )}
          {generator === "position" && (
            <NumberSetting
              label="Starting finger"
              value={opts.startingFinger}
              min={1}
              max={5}
              commit={(n) => option({ startingFinger: n })}
            />
          )}
          {generator === "intervals" && (
            <fieldset>
              <legend>Diatonic intervals</legend>
              {[2, 3, 4, 5, 6, 7].map((i) => (
                <label key={i}>
                  <input
                    type="checkbox"
                    checked={opts.intervals.includes(i)}
                    onChange={(e) =>
                      command("practice.option", {
                        field: "intervals",
                        value: i,
                        enabled: e.target.checked,
                      })
                    }
                  />
                  {i === 2 ? "2nd" : i === 3 ? "3rd" : `${i}th`}
                </label>
              ))}
            </fieldset>
          )}
          {symbol && (
            <>
              {check("Random chords", opts.randomChords, (v) =>
                option({ randomChords: v }),
              )}
              {check("Multiple keys", opts.multiKey, (v) =>
                option({ multiKey: v }),
              )}
              {select(
                "Chord tones",
                String(opts.tones),
                [
                  ["3", "3 tones"],
                  ["4", "4 tones"],
                ],
                (v) => option({ tones: Number(v) }),
              )}
              {select(
                "Common notes",
                String(opts.commonNotes),
                [
                  ["0", "Any"],
                  ["1", "At least 1"],
                  ["2", "At least 2"],
                ],
                (v) => option({ commonNotes: Number(v) }),
              )}
              <span className="helper">
                Common notes compare pitch classes through{" "}
                {pianoKeys().find((k) => k.midi === opts.ignoreAbove)?.label}{" "}
                (MIDI {opts.ignoreAbove}); octaves count as the same class.
              </span>
              <NumberSetting
                label="Ignore common notes above MIDI"
                value={opts.ignoreAbove}
                min={21}
                max={108}
                commit={(n) => option({ ignoreAbove: n })}
              />
              {opts.multiKey && (
                <fieldset>
                  <legend>Chord keys</legend>
                  {keys.slice(0, 12).map((k) => (
                    <label key={k}>
                      <input
                        type="checkbox"
                        checked={opts.keys.includes(k)}
                        onChange={(e) =>
                          command("practice.option", {
                            field: "keys",
                            value: k,
                            enabled: e.target.checked,
                          })
                        }
                      />
                      {k}
                    </label>
                  ))}
                </fieldset>
              )}
            </>
          )}
        </div>
      </details>
    </section>
  );
}
