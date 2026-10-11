import { useEffect, useRef, useState, type ReactNode } from "react";
import { NativeProvider, useNative } from "./native";
import type { AudioSettings, MetronomeSettings } from "./types";
import { PianoKeyboard } from "./PianoKeyboard";
import { StaffPractice } from "./StaffPractice";
import { PlayAlong } from "./PlayAlong";
import "./style.css";
const routes = [
  ["staff", "Staff", "01"],
  ["ear", "Ear Training", "02"],
  ["flash", "Flash Cards", "03"],
  ["play", "Play Along", "04"],
  ["guide", "Guide", "05"],
] as const;
// Local text drafts allow normal editing; only valid committed values reach C++.
function NumericInput({
  value,
  min,
  max,
  integer = false,
  onCommit,
}: {
  value: number;
  min: number;
  max: number;
  integer?: boolean;
  onCommit: (value: number) => void;
}) {
  const focused = useRef(false);
  const [draft, setDraft] = useState(String(value));
  const [error, setError] = useState<string | null>(null);
  useEffect(() => {
    if (!focused.current) setDraft(String(value));
  }, [value]);
  const commit = () => {
    focused.current = false;
    const n = Number(draft);
    if (
      draft.trim() &&
      Number.isFinite(n) &&
      n >= min &&
      n <= max &&
      (!integer || Number.isInteger(n))
    ) {
      setError(null);
      onCommit(n);
    } else {
      setDraft(String(value));
      setError(
        `Enter ${integer ? "a whole number" : "a number"} from ${min} to ${max}.`,
      );
    }
  };
  return (
    <>
      <input
        type="number"
        min={min}
        max={max}
        step={integer ? 1 : "any"}
        value={draft}
        aria-invalid={!!error}
        onFocus={() => {
          focused.current = true;
          setError(null);
        }}
        onChange={(e) => setDraft(e.target.value)}
        onBlur={commit}
        onKeyDown={(e) => {
          if (e.key === "Enter") {
            e.preventDefault();
            e.currentTarget.blur();
          }
        }}
      />
      {error && (
        <small className="field-error" role="alert">
          {error}
        </small>
      )}
    </>
  );
}
function Dialog({
  title,
  close,
  children,
}: {
  title: string;
  close: () => void;
  children: ReactNode;
}) {
  const panel = useRef<HTMLDivElement>(null);
  useEffect(() => {
    const previous = document.activeElement as HTMLElement | null;
    panel.current?.querySelector<HTMLElement>("button,input,select")?.focus();
    const key = (e: KeyboardEvent) => {
      if (e.key === "Escape") close();
      if (e.key === "Tab") {
        const elements = [
          ...document.querySelectorAll<HTMLElement>(
            ".global-toolbar button,.global-toolbar input,.global-dialog button,.global-dialog input,.global-dialog select",
          ),
        ].filter((el) => !el.hasAttribute("disabled"));
        const i = elements.indexOf(document.activeElement as HTMLElement);
        if (elements.length) {
          e.preventDefault();
          elements[
            (i + (e.shiftKey ? -1 : 1) + elements.length) % elements.length
          ].focus();
        }
      }
    };
    document.addEventListener("keydown", key);
    return () => {
      document.removeEventListener("keydown", key);
      previous?.focus();
    };
  }, [close]);
  return (
    <div
      className="dialog-backdrop"
      onClick={(e) => {
        if (e.target === e.currentTarget) close();
      }}
    >
      <div
        className="global-dialog"
        role="dialog"
        aria-labelledby="dialog-title"
        ref={panel}
      >
        <div className="dialog-heading">
          <div>
            <span className="eyebrow">GLOBAL CONFIGURATION</span>
            <h2 id="dialog-title">{title}</h2>
          </div>
          <button onClick={close} aria-label="Close settings">
            ×
          </button>
        </div>
        {children}
      </div>
    </div>
  );
}
function AudioDialog() {
  const native = useNative(),
    s = native.state;
  if (!s) return null;
  const settings = s.pendingChange ?? s.settings;
  const set = (p: Partial<AudioSettings>) => {
    void native.command("audio.settings", p).catch(() => {});
  };
  return (
    <>
      <p className="helper">
        Your input and sound settings apply everywhere. Device changes wait
        until live keys and sustain are released.
      </p>
      <div className="form-grid">
        <label>
          Output mode
          <select
            value={settings.outputMode}
            onChange={(e) =>
              set({ outputMode: e.target.value as AudioSettings["outputMode"] })
            }
          >
            <option value="internal">Sampled piano</option>
            <option value="external">External MIDI instrument</option>
            <option value="silent">Silent monitoring</option>
          </select>
        </label>
        <label>
          WASAPI output
          <select
            value={settings.outputDevice}
            onChange={(e) => set({ outputDevice: Number(e.target.value) })}
          >
            <option value={-1}>System default</option>
            {s.audioOutputs.map((d) => (
              <option key={d.id} value={d.id}>
                {d.name}
              </option>
            ))}
          </select>
        </label>
        <label>
          External MIDI output
          <select
            value={settings.externalDevice}
            onChange={(e) => set({ externalDevice: Number(e.target.value) })}
          >
            <option value={-1}>None</option>
            {s.midiOutputs.map((d) => (
              <option key={d.id} value={d.id}>
                {d.name}
              </option>
            ))}
          </select>
        </label>
        <label>
          Velocity response <output>{settings.velocityCurve.toFixed(2)}</output>
          <input
            type="range"
            min="0.25"
            max="4"
            step="0.05"
            value={settings.velocityCurve}
            onChange={(e) => set({ velocityCurve: Number(e.target.value) })}
          />
        </label>
      </div>
      <label className="check">
        <input
          type="checkbox"
          checked={settings.monitoring}
          onChange={(e) => set({ monitoring: e.target.checked })}
        />{" "}
        Live piano monitoring
      </label>
      <label className="check">
        <input
          type="checkbox"
          checked={settings.exclusive}
          onChange={(e) => set({ exclusive: e.target.checked })}
        />{" "}
        Exclusive WASAPI mode
      </label>
      <h3>MIDI inputs</h3>
      {s.devices.length ? (
        s.devices.map((d) => (
          <label key={d.id} className="device">
            <input
              type="checkbox"
              checked={d.enabled}
              onChange={(e) => {
                void native
                  .command("devices.enable", {
                    id: d.id,
                    enabled: e.target.checked,
                  })
                  .catch(() => {});
              }}
            />
            <span>
              {d.name}
              <small>
                {d.connected
                  ? "Connected · live monitoring"
                  : "Disabled or disconnected"}
              </small>
            </span>
            <span className={d.connected ? "status-dot ready" : "status-dot"} />
          </label>
        ))
      ) : (
        <p className="empty-note">
          No MIDI input detected. Connect your keyboard; the native service
          scans automatically.
        </p>
      )}
      <h3>Input channels</h3>
      <div className="channel-grid">
        {Array.from({ length: 16 }, (_, i) => (
          <label key={i}>
            <input
              type="checkbox"
              checked={settings.channels.includes(i)}
              onChange={(e) =>
                void native
                  .command("audio.channel", {
                    channel: i,
                    enabled: e.target.checked,
                  })
                  .catch(() => {})
              }
            />
            {i + 1}
          </label>
        ))}
      </div>
      {s.pendingChange && (
        <div className="notice">
          Change pending while live notes are held.
          <button
            onClick={() => {
              void native
                .command("audio.applyPending", { applyNow: true })
                .catch(() => {});
            }}
          >
            Apply now · interrupts live notes
          </button>
        </div>
      )}
      <div className="technical">
        <span>{s.output.name || "No active audio output"}</span>
        <span>
          {s.output.actualSampleRate || "—"} Hz · {s.output.actualFrames || "—"}{" "}
          frames · API latency {s.output.latencyMs.toFixed(1)} ms
        </span>
      </div>
      <details>
        <summary>Native MIDI monitor</summary>
        <div className="monitor">
          {s.monitor.slice(-12).map((m, i) => (
            <code key={i}>
              source {m.sourceId} · {m.status.toString(16)} {m.data1} {m.data2}{" "}
              · frame {m.sampleFrame}
            </code>
          ))}
        </div>
      </details>
    </>
  );
}
function MetronomeDialog() {
  const native = useNative(),
    s = native.state;
  const taps = useRef<number[]>([]);
  if (!s) return null;
  const m = s.metronome.settings;
  const set = (p: Partial<MetronomeSettings>) => {
    void native.command("metronome.config", p).catch(() => {});
  };
  const meter = (params: Partial<MetronomeSettings["meter"]>) => {
    void native.command("metronome.meter", params).catch(() => {});
  };
  const tap = () => {
    const now = performance.now();
    if (taps.current.length && now - taps.current.at(-1)! > 2500)
      taps.current = [];
    taps.current = [...taps.current.slice(-5), now];
    if (taps.current.length > 1) {
      const bpm = Math.round(
        (60000 * (taps.current.length - 1)) / (now - taps.current[0]),
      );
      set({ tempo: Math.max(20, Math.min(300, bpm)) });
    }
  };
  return (
    <>
      <p className="helper">
        A native sample-clock pulse that stays with you across every exercise,
        dialog and reload.
      </p>
      <div className="tempo-editor">
        <label>
          Quarter-note BPM
          <NumericInput
            value={m.tempo}
            min={20}
            max={300}
            onCommit={(n) => set({ tempo: n })}
          />
        </label>
        <button onClick={tap}>Tap tempo</button>
        <input
          aria-label="Metronome tempo"
          type="range"
          min="20"
          max="300"
          value={m.tempo}
          onChange={(e) => set({ tempo: Number(e.target.value) })}
        />
      </div>
      <div className="form-grid">
        <label>
          Clock mode
          <select
            value={m.mode}
            onChange={(e) =>
              set({ mode: e.target.value as MetronomeSettings["mode"] })
            }
          >
            <option value="independent">Independent</option>
            <option value="follow">Follow practice / song</option>
          </select>
        </label>
        <label className="wide-field">
          Beat divisions
          <select
            value={m.subdivision}
            onChange={(e) =>
              set({
                subdivision: e.target.value as MetronomeSettings["subdivision"],
              })
            }
          >
            <option value="quarter">1 pulse per denominator beat</option>
            <option value="eighth">2 pulses per denominator beat</option>
            <option value="sixteenth">4 pulses per denominator beat</option>
            <option value="triplet">
              3 pulses per denominator beat (triplet)
            </option>
          </select>
        </label>
        <label>
          Beats per bar
          <NumericInput
            value={m.meter.numerator}
            min={1}
            max={12}
            integer
            onCommit={(n) => meter({ numerator: n })}
          />
        </label>
        <label>
          Beat unit
          <select
            value={m.meter.denominator}
            onChange={(e) =>
              meter({ denominator: Number(e.target.value) as 2 | 4 | 8 | 16 })
            }
          >
            {[2, 4, 8, 16].map((n) => (
              <option key={n} value={n}>
                1/{n} note
              </option>
            ))}
          </select>
        </label>
        <label>
          Click sound
          <select
            value={m.sound}
            onChange={(e) =>
              set({ sound: e.target.value as MetronomeSettings["sound"] })
            }
          >
            <option value="click">Click</option>
            <option value="woodblock">Woodblock</option>
            <option value="rim">Rim</option>
          </select>
        </label>
        <label>
          Count-in bars
          <select
            value={m.countIn}
            onChange={(e) => set({ countIn: Number(e.target.value) })}
          >
            {[0, 1, 2, 3, 4].map((n) => (
              <option key={n}>{n}</option>
            ))}
          </select>
        </label>
        <label>
          Swing <output>{m.swing}%</output>
          <input
            type="range"
            min="50"
            max="75"
            value={m.swing}
            onChange={(e) => set({ swing: Number(e.target.value) })}
          />
        </label>
        <label>
          Click volume <output>{Math.round(m.volume * 100)}%</output>
          <input
            type="range"
            min="0"
            max="1"
            step="0.01"
            value={m.volume}
            onChange={(e) => set({ volume: Number(e.target.value) })}
          />
        </label>
      </div>
      <p className="helper">
        {m.meter.numerator}/{m.meter.denominator}: {m.meter.numerator} beats of
        a 1/{m.meter.denominator} note. BPM always measures quarter notes. Tempo
        changes on the next beat; meter, divisions and swing on the next bar.
        Count-in edits take effect on the next start.
      </p>
      <h3>Beat accents</h3>
      <div className="accent-grid">
        {m.accents.map((a, i) => (
          <label key={i}>
            Beat {i + 1}
            <select
              aria-label={`Beat ${i + 1} accent`}
              value={a}
              onChange={(e) =>
                void native
                  .command("metronome.accent", {
                    beat: i,
                    accent: e.target.value,
                  })
                  .catch(() => {})
              }
            >
              <option value="strong">Strong</option>
              <option value="weak">Weak</option>
              <option value="mute">Mute</option>
            </select>
          </label>
        ))}
      </div>
      <div className="dialog-actions">
        <label className="check">
          <input
            type="checkbox"
            checked={m.mute}
            onChange={(e) => set({ mute: e.target.checked })}
          />{" "}
          Mute clicks
        </label>
        <button
          className="primary"
          onClick={() => {
            void native
              .command(
                s.metronome.running ? "metronome.stop" : "metronome.start",
              )
              .catch(() => {});
          }}
        >
          {s.metronome.running ? "Stop metronome" : "Start metronome"}
        </button>
      </div>
    </>
  );
}
function Workspace({ route }: { route: string }) {
  if (route === "guide")
    return (
      <section className="guide">
        <span className="eyebrow">YOUR LOCAL STUDIO</span>
        <h1>
          Make a little progress,
          <br />
          every day.
        </h1>
        <p className="lead">
          A sampled piano, a steady pulse, and room to focus.
        </p>
        <div className="guide-grid">
          <article>
            <span>01</span>
            <h3>Connect & play</h3>
            <p>
              Connect a MIDI keyboard. The native service automatically opens
              enabled devices and monitors them on every tab. Audio settings
              show the actual WASAPI output and sample format.
            </p>
          </article>
          <article>
            <span>02</span>
            <h3>Keep your pulse</h3>
            <p>
              Open metronome settings with M. BPM is measured in quarter notes;
              beat units follow the time-signature denominator. Stopping an
              exercise leaves your metronome running.
            </p>
          </article>
          <article>
            <span>03</span>
            <h3>Stay local</h3>
            <p>
              Your settings are saved in the native SQLite database. Reloading
              or closing this browser leaves hardware input and clicks running.
              Stop the native service to shut down sound.
            </p>
          </article>
          <article>
            <span>04</span>
            <h3>Recover gracefully</h3>
            <p>
              Panic explicitly clears all notes and clicks. Audio-device changes
              defer while notes or sustain are held. Apply now intentionally
              interrupts live notes.
            </p>
          </article>
        </div>
        <p className="helper">
          No browser audio. No account. No external fonts or cloud assets.
          Piano: Salamander Grand Piano by Alexander Holm, CC BY 3.0.
        </p>
      </section>
    );
  const r = routes.find((r) => r[0] === route) ?? routes[0];
  return (
    <section className="workspace">
      <div className="workspace-heading">
        <div>
          <span className="eyebrow">PRACTICE ROOM / {r[2]}</span>
          <h1>{r[1]}</h1>
        </div>
        <span className="workspace-tag">LOCAL PIANO</span>
      </div>
      <div className="workspace-stage">
        {route === "staff" ? (
          <StaffPractice />
        ) : route === "play" ? (
          <PlayAlong />
        ) : (
          <>
            <div className="staff-decoration" aria-hidden="true">
              <i />
              <i />
              <i />
              <i />
              <i />
              <span>𝄞</span>
            </div>
            <div className="stage-copy">
              <span className="eyebrow">YOUR PRACTICE WORKSPACE</span>
              <h2>Listen. Notice. Play.</h2>
              <p>
                {route === "staff"
                  ? "Reading and keyboard practice will live here."
                  : route === "ear"
                    ? "Interval and melody exercises will live here."
                    : route === "flash"
                      ? "Note and chord recall will live here."
                      : "Your local MIDI library and playback will live here."}{" "}
                Global piano and metronome controls are ready above.
              </p>
            </div>
          </>
        )}
      </div>
      <div className="workspace-footer">
        <span>Sound stays with you across every room.</span>
        <span>Native engine · offline</span>
      </div>
    </section>
  );
}
function Shell() {
  const toolbar = useRef<HTMLElement>(null);
  useEffect(() => {
    const update = () =>
      document.documentElement.style.setProperty(
        "--toolbar-height",
        `${toolbar.current?.offsetHeight ?? 84}px`,
      );
    const observer = new ResizeObserver(update);
    if (toolbar.current) observer.observe(toolbar.current);
    update();
    return () => observer.disconnect();
  }, []);
  const native = useNative(),
    s = native.state;
  const [route, setRoute] = useState(location.hash.slice(1) || "staff");
  const [dialog, setDialog] = useState<"audio" | "metronome" | null>(null);
  const close = useRef(() => setDialog(null)).current;
  useEffect(() => {
    const change = () => setRoute(location.hash.slice(1) || "staff");
    const shortcut = (e: KeyboardEvent) => {
      const el = e.target as HTMLElement;
      if (
        e.key.toLowerCase() === "m" &&
        !e.ctrlKey &&
        !e.altKey &&
        !e.metaKey &&
        !el.closest('input,textarea,select,[contenteditable="true"]')
      ) {
        e.preventDefault();
        setDialog("metronome");
      }
    };
    window.addEventListener("hashchange", change);
    window.addEventListener("keydown", shortcut);
    return () => {
      window.removeEventListener("hashchange", change);
      window.removeEventListener("keydown", shortcut);
    };
  }, []);
  const cmd = (type: string, p: Record<string, unknown> = {}) => {
    void native.command(type, p).catch(() => {});
  };
  const ready = !!native.connected && !!s?.audioReady && !!s?.pianoReady;
  return (
    <>
      <header className="global-toolbar" ref={toolbar}>
        <a href="#staff" className="brand">
          <span className="brand-icon">♬</span>
          <span>
            Sight Reading
            <small>
              {!native.connected
                ? "CONNECTING"
                : s?.pianoLoading
                  ? "LOADING PIANO"
                  : ready
                    ? "NATIVE PIANO READY"
                    : "ENGINE NEEDS ATTENTION"}
            </small>
          </span>
        </a>
        <div className="engine-status" role="status">
          <span className={`status-dot ${ready ? "ready" : ""}`} />
          <span>
            {!native.connected
              ? "Connecting"
              : s?.pianoLoading
                ? "Loading piano"
                : ready
                  ? "Piano ready"
                  : s?.error
                    ? "Engine needs attention"
                    : "Audio unavailable"}
            <small>
              {s?.devices.filter((d) => d.connected).length ?? 0} MIDI inputs ·{" "}
              {s?.output.actualFrames || "—"} frames
            </small>
          </span>
        </div>
        <div className="toolbar-controls">
          <button
            onClick={() => setDialog("audio")}
            aria-label="Audio and MIDI settings"
          >
            Audio / MIDI <span>⚙</span>
          </button>
          <label className="monitor-toggle">
            <input
              type="checkbox"
              disabled={!s || !native.connected}
              checked={s?.settings.monitoring ?? false}
              onChange={(e) =>
                cmd("audio.settings", { monitoring: e.target.checked })
              }
            />
            Monitor
          </label>
          <label className="master-volume">
            <span>Volume</span>
            <input
              aria-label="Master volume"
              type="range"
              min="0"
              max="1"
              step="0.01"
              disabled={!s || !native.connected}
              value={s?.settings.volume ?? 0.7}
              onChange={(e) =>
                cmd("audio.settings", { volume: Number(e.target.value) })
              }
            />
          </label>
          <div className="metro-control">
            <button
              disabled={!s || !native.connected}
              className={s?.metronome.running ? "metro-running" : ""}
              onClick={() =>
                cmd(s?.metronome.running ? "metronome.stop" : "metronome.start")
              }
              aria-label={
                s?.metronome.running ? "Stop metronome" : "Start metronome"
              }
            >
              {s?.metronome.running ? "■" : "▶"}{" "}
              <strong>{s?.metronome.settings.tempo ?? 100}</strong>
              <span>BPM</span>
            </button>
            <button
              aria-label="Metronome settings"
              onClick={() => setDialog("metronome")}
            >
              ⚙
            </button>
          </div>
          <button
            className="panic"
            disabled={!native.connected}
            onClick={() => cmd("panic")}
          >
            Panic
          </button>
          <button
            className="fullscreen"
            aria-label="Toggle fullscreen"
            onClick={() => {
              void (
                document.fullscreenElement
                  ? document.exitFullscreen()
                  : document.documentElement.requestFullscreen()
              ).catch(() => {});
            }}
          >
            ⛶
          </button>
        </div>
      </header>
      <div className="app-body">
        <nav className="side-nav" aria-label="Practice tabs">
          <span className="nav-label">EXPLORE</span>
          {routes.map(([id, title, index]) => (
            <a
              key={id}
              className={route === id ? "active" : ""}
              aria-current={route === id ? "page" : undefined}
              href={`#${id}`}
            >
              <span>{index}</span>
              {title}
              <b>↗</b>
            </a>
          ))}
          <div className="nav-bottom">
            <span className="tiny-piano">▮▯▮▯▮</span>
            <p>
              A quiet space
              <br />
              to become fluent.
            </p>
            <small>100% local · native sound</small>
          </div>
        </nav>
        <main>
          <Workspace route={route} />
          <PianoKeyboard />
        </main>
      </div>
      {(native.error || s?.error) && (
        <div className="error-banner" role="alert">
          <span>{native.error || s?.error}</span>
          {native.error && (
            <button
              onClick={() => native.clearError()}
              aria-label="Dismiss error"
            >
              ×
            </button>
          )}
        </div>
      )}
      {!!s?.overload && (
        <div className="overload" role="status">
          Native admission / event pressure: {s.overload} cumulative incidents.{" "}
          Sampler capacity refusals: {s.voiceRefusals}. Existing live voices
          remain protected.
        </div>
      )}
      {dialog && (
        <Dialog
          title={dialog === "audio" ? "Audio & MIDI" : "Metronome"}
          close={close}
        >
          {dialog === "audio" ? <AudioDialog /> : <MetronomeDialog />}
        </Dialog>
      )}
    </>
  );
}
export default function App() {
  return (
    <NativeProvider>
      <Shell />
    </NativeProvider>
  );
}
