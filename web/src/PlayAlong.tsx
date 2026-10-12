import { useEffect, useMemo, useRef, useState } from "react";
import { useNative } from "./native";
import { Library } from "./Library";
import { songApi } from "./songApi";
import { EngravedScore } from "./EngravedScore";
import type { Song, SongTrack, TimelineNote, TransportConfig } from "./types";
import "./playAlong.css";
import { NumericInput } from "./NumericInput";
import { PlayerControls, RestartButton } from "./PlayerControls";
export function PlayAlong({ onFocus }: { onFocus: () => void }) {
  const native = useNative(),
    p = native.state?.transport;
  const dragPause = useRef<Promise<void>>(Promise.resolve());
  const [seekPreview, setSeekPreview] = useState<number | null>(null);
  useEffect(() => {
    if (seekPreview !== null && p && Math.abs(p.songSeconds - seekPreview) < .001)
      setSeekPreview(null);
  }, [p?.songSeconds, seekPreview]);
  const [song, setSong] = useState<Song | null>(null),
    [notes, setNotes] = useState<TimelineNote[]>([]),
    [error, setError] = useState("");
  const run = async (type: string, params: Record<string, unknown> = {}) => {
    setError("");
    try {
      await native.command(type, params);
    } catch (e) {
      setError((e as Error).message);
    }
  };
  const choose = async (s: Song) => {
    setSeekPreview(null);
    setNotes([]);
    await run("transport.load", { id: s.id });
    try {
      // Library rows are summaries; engraving requires the complete meter/tempo/key maps.
      setSong(await songApi<Song>(`/${s.id}`));
    } catch (e) { setError((e as Error).message); }
  };
  useEffect(() => {
    if (p?.songId && p.songId !== song?.id)
      void songApi<Song>(`/${p.songId}`)
        .then(setSong)
        .catch((e) => setError(e.message));
  }, [p?.songId, song?.id]);
  const bucket = song && p ? Math.floor(p.songTick / (song.ppq * 8)) : 0;
  const [scoreWindow,setScoreWindow]=useState({from:0,to:0});
  useEffect(() => {
    if (!song) return;
    let current = true;
    void songApi<{
      notes: TimelineNote[];
      measures: { number: number; startFrame: number }[];
      truncated?: boolean;
    }>(
      `/${song.id}/events?fromTick=${Math.max(0, (bucket - 2) * song.ppq * 8)}&toTick=${(bucket + 6) * song.ppq * 8}`,
    )
      .then((s) => {
        if (current) {
          setNotes(s.notes);
          setScoreWindow({from:Math.max(0,(bucket-2)*song.ppq*8),to:(bucket+6)*song.ppq*8});
          if (s.truncated)
            setError(
              "Dense notation window exceeded 4096 notes; the displayed window is truncated. Original playback remains intact.",
            );
        }
      })
      .catch((e) => setError(e.message));
    return () => {
      current = false;
    };
  }, [song?.id, bucket, p?.revision]);
  const config = (edit: Partial<TransportConfig>) =>
    void run("transport.configure", edit);
  const patch = async (edit: Record<string, unknown>) => {
    if (!song) return;
    try {
      const s = await songApi<Song>(`/${song.id}`, {
        method: "PATCH",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(edit),
      });
      setSong(s);
      await run("transport.load", { id: s.id });
    } catch (e) {
      setError((e as Error).message);
    }
  };
  const track = (t: SongTrack, key: string, value: unknown) =>
    void patch({ tracks: [{ id: t.id, [key]: value }] });
  const displayedNotes = useMemo(
    () =>
      notes.map((n) =>
        p?.config.hand &&
        p.config.hand !== "both" &&
        n.staff !== (p.config.hand === "left" ? "bass" : "treble")
          ? { ...n, highlight: "muted" as const }
          : n,
      ),
    [notes, p?.config.hand],
  );
  return (
    <div className="play-along">
      <Library selected={song?.id ?? ""} choose={(s) => void choose(s)} />
      <section className="song-workspace">
        {error && <p role="alert">{error}</p>}
        {p?.error && <p role="alert">{p.error}</p>}
        {p?.accompanimentError && <p role="alert">{p.accompanimentError}</p>}
        {p?.followError && <p role="status">{p.followError}</p>}
        {!song || !p ? (
          <p>Choose a starter arrangement or import a MIDI file.</p>
        ) : (
          <>
            <label>
              Song title
              <input
                aria-label="Song title"
                key={song.id + song.title}
                defaultValue={song.title}
                onBlur={(e) => {
                  if (e.target.value !== song.title)
                    void patch({ title: e.target.value });
                }}
              />
            </label>
            <div className="transport-buttons">
              <label>
                <input type="checkbox" checked={p.config.mode === "wait"}
                  onChange={(e) => config({ mode: e.target.checked ? "wait" : "continuous" })} />
                Wait mode
              </label>
              <label>
                <input type="checkbox" checked={p.config.hints ?? false}
                  onChange={(e) => config({ hints: e.target.checked })} />
                Hint mode
              </label>
              <label>
                <input
                  type="checkbox"
                  checked={p.config.hearPractice}
                  onChange={(e) => config({ hearPractice: e.target.checked })}
                />
                Hear practice part
              </label>

              <button onClick={() => void run("transport.stop")}>Stop</button>
              <button
                onClick={() =>
                  void document.documentElement.requestFullscreen()
                }
              >
                Fullscreen
              </button>
              <button className="focus-mode-trigger" onClick={onFocus}>Focus mode</button>
            </div>
            {p.waiting && (
              <p className="retry" role="status">
                Waiting for the selected hand{p.config.hand === "both" ? "s" : ""}: press {p.target.join(" + ")} to continue.
              </p>
            )}
            {p.retry && !p.waiting && (
              <p className="retry" role="status">
                Paused on miss — play a fresh target chord:{" "}
                {p.target.join(" + ")}. Backing resumes after your answer.
              </p>
            )}
            <details className="song-settings">
              <summary>Playback settings, loops & tracks</summary>
              <div className="song-options">
                <label>
                  Tempo BPM
                  <NumericInput
                    ariaLabel="Song tempo"
                    min={20}
                    max={300}
                    value={p.tempo}
                    onCommit={(bpm) => void run("transport.tempo", { bpm })}
                  />
                </label>
                <label>
                  Speed
                  <input
                    aria-label="Speed"
                    type="number"
                    min=".25"
                    max="4"
                    step=".05"
                    value={p.config.speed}
                    onChange={(e) => config({ speed: Number(e.target.value) })}
                  />
                </label>
                <label>
                  Score spacing
                  <input
                    aria-label="Song spacing"
                    type="range"
                    min="30"
                    max={300}
                    value={p.config.spacing}
                    onChange={(e) =>
                      config({ spacing: Number(e.target.value) })
                    }
                  />
                </label>
                <label>
                  <input
                    type="checkbox"
                    checked={p.config.practiceEnabled}
                    onChange={(e) =>
                      config({ practiceEnabled: e.target.checked })
                    }
                  />
                  Score practice
                </label>
                <label>
                  Practice mode
                  <select
                    value={p.config.mode}
                    onChange={(e) =>
                      config({
                        mode: e.target.value as TransportConfig["mode"],
                      })
                    }
                  >
                    <option value="continuous">Continuous timing</option>
                    <option value="wait">Wait for correct keys</option>
                    <option value="pauseOnMiss">Pause on miss</option>
                  </select>
                </label>
                <label>
                  Tolerance ± ms
                  <input
                    type="number"
                    aria-label="Song tolerance"
                    min={20}
                    max="1000"
                    value={p.config.toleranceMs}
                    onChange={(e) =>
                      config({ toleranceMs: Number(e.target.value) })
                    }
                  />
                </label>
              </div>
              <div className="song-options">
                <label>
                  <input
                    type="checkbox"
                    checked={p.config.loop}
                    onChange={(e) => config({ loop: e.target.checked })}
                  />
                  A–B loop
                </label>
                <label>
                  A tick
                  <input
                    type="number"
                    aria-label="Loop A"
                    min="0"
                    max={song.durationTicks}
                    value={p.config.a}
                    onChange={(e) => config({ a: Number(e.target.value) })}
                  />
                </label>
                <button onClick={() => config({ a: Math.floor(p.songTick) })}>
                  Set A here
                </button>
                <label>
                  B tick
                  <input
                    type="number"
                    aria-label="Loop B"
                    min="0"
                    max={song.durationTicks}
                    value={p.config.b}
                    onChange={(e) => config({ b: Number(e.target.value) })}
                  />
                </label>
                <button onClick={() => config({ b: Math.floor(p.songTick) })}>
                  Set B here
                </button>
              </div>
              <div className="song-options">
                <label>
                  Auto accompaniment
                  <select
                    aria-label="Auto accompaniment"
                    value={p.config.accompaniment}
                    onChange={(e) => config({ accompaniment: e.target.value })}
                  >
                    {[
                      "Off",
                      "Root",
                      "Triad",
                      "Root+5",
                      "Arp",
                      "Bossa Nova",
                    ].map((v) => (
                      <option key={v}>{v}</option>
                    ))}
                  </select>
                </label>
                <label>
                  Accompaniment rate
                  <input
                    aria-label="Accompaniment rate"
                    type="number"
                    min=".25"
                    max="4"
                    step=".25"
                    value={p.config.rate}
                    onChange={(e) => config({ rate: Number(e.target.value) })}
                  />
                </label>
                <label>
                  Voicing spacing
                  <select
                    value={p.config.accompanimentSpacing}
                    onChange={(e) =>
                      config({ accompanimentSpacing: Number(e.target.value) })
                    }
                  >
                    <option value="1">Close</option>
                    <option value="2">Open</option>
                  </select>
                </label>
              </div>
              <div className="song-tracks">
                {song.tracks.map((t) => (
                  <div key={t.id}>
                    <strong>{t.title}</strong>
                    <small>
                      {t.noteCount} notes
                      {t.percussion ? " · percussion, disabled by default" : ""}
                    </small>
                    <label>
                      <input
                        type="checkbox"
                        checked={t.practice}
                        onChange={(e) => track(t, "practice", e.target.checked)}
                      />
                      Practice
                    </label>
                    <label>
                      <input
                        type="checkbox"
                        checked={t.backing}
                        onChange={(e) => track(t, "backing", e.target.checked)}
                      />
                      Backing
                    </label>
                    <label>
                      Staff
                      <select
                        value={t.staff}
                        onChange={(e) => track(t, "staff", e.target.value)}
                      >
                        {["split", "treble", "bass", "grand"].map((v) => (
                          <option key={v}>{v}</option>
                        ))}
                      </select>
                    </label>
                  </div>
                ))}
              </div>
            </details>
            <div className="practice-player song-player">
              <PlayerControls
                hand={p.config.hand ?? "both"}
                onHand={(hand) => config({ hand })}
                playing={p.state === "playing" || p.waiting}
                playLabel={p.state === "playing" || p.waiting ? "Pause" : "Play"}
                onPlay={() =>
                  void run(
                    p.state === "playing" || p.waiting
                      ? "transport.pause"
                      : "transport.play",
                  )
                }
              />
              <div className="practice-score">
                <EngravedScore
                  key={song.id}
                  song={song}
                  fromTick={scoreWindow.from}
                  toTick={scoreWindow.to}
                  notes={displayedNotes}
                  sampleRate={p.sampleRate}
                  clockFrame={() =>
                    (seekPreview ?? p.songSeconds) * p.sampleRate +
                    (p.state === "playing"
                      ? Math.max(0, native.frame() - p.sampleFrame) * p.speed
                      : 0)
                  }
                  pixelsPerSecond={p.config.spacing}
                  running={p.state === "playing"}
                  label="Song practice notation"
                  paper
                  scrub={{
                    durationFrames: song.durationSeconds * p.sampleRate,
                    start: () => { dragPause.current = run("transport.pause"); },
                    seek: async (frame) => {
                      const seconds = frame / p.sampleRate;
                      setSeekPreview(seconds);
                      await dragPause.current;
                      try {
                        await native.command("transport.seek", { seconds });
                      } catch (e) {
                        setSeekPreview(null);
                        setError((e as Error).message);
                      }
                    },
                  }}
                />
                <RestartButton
                  label="Restart"
                  onClick={() => void run("transport.restart")}
                />
              </div>
              <div className="practice-progress song-progress">
                <input
                  aria-label="Song position"
                  type="range"
                  min="0"
                  max={song.durationTicks}
                  step="1"
                  value={p.songTick}
                  onChange={(e) =>
                    void run("transport.seek", { tick: Number(e.target.value) })
                  }
                />
              </div>
            </div>
            <div className="scoreline">
              <span>{p.hits} hits</span>
              <span>{p.misses} misses</span>
              <span>{p.accuracy.toFixed(0)}% accuracy</span>
              <span>{p.state}</span>
              <span>
                Position · {Math.round(p.songSeconds)} /{" "}
                {Math.round(song.durationSeconds)} sec
              </span>
            </div>
          </>
        )}
      </section>
    </div>
  );
}
