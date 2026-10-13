import { useEffect, useMemo, useRef, useState } from "react";
import { useNative } from "./native";
import { Library } from "./Library";
import { songApi } from "./songApi";
import { EngravedScore } from "./EngravedScore";
import type { Song, SongTrack, TimelineNote, TransportConfig } from "./types";
import "./playAlong.css";
import { NumericInput } from "./NumericInput";
import { PlayerControls, RestartButton } from "./PlayerControls";
import { LoopMarkers, type LoopSelection } from "./LoopMarkers";
import { returnPosition } from "./clock";
import { FingeringControls } from "./FingeringControls";
export function PlayAlong({ onFocus }: { onFocus: () => void }) {
  const native = useNative(),
    p = native.state?.transport;
  const dragPause = useRef<Promise<void>>(Promise.resolve());
  const [seekPreview, setSeekPreview] = useState<number | null>(null);
  const overview = useRef<HTMLDivElement>(null);
  const [hideFingerings, setHideFingerings] = useState(true);
  const [hidePassage, setHidePassage] = useState(true);
  const [hidingPassage, setHidingPassage] = useState(false);
  useEffect(() => {
    if (seekPreview !== null && p && Math.abs(p.songSeconds - seekPreview) < .001)
      setSeekPreview(null);
  }, [p?.songSeconds, seekPreview]);
  const [song, setSong] = useState<Song | null>(null),
    [notes, setNotes] = useState<TimelineNote[]>([]),
    [error, setError] = useState("");
  const activeSong = useRef("");
  const [selectedFingerNotes, setSelectedFingerNotes] = useState<string[] | null>(null);
  const run = async (type: string, params: Record<string, unknown> = {}) => {
    setError("");
    try {
      await native.command(type, params);
    } catch (e) {
      setError((e as Error).message);
    }
  };
  const choose = async (s: Song) => {
    activeSong.current=s.id;
    setSelectedFingerNotes(null);
    setSeekPreview(null);
    setNotes([]);
    await run("transport.load", { id: s.id });
    try {
      // Library rows are summaries; engraving requires the complete meter/tempo/key maps.
      const loaded=await songApi<Song>(`/${s.id}`);
      if(activeSong.current===s.id)setSong(previous=>previous?.id===loaded.id && (previous.fingeringRevision ?? 0)>(loaded.fingeringRevision ?? 0) ? previous : loaded);
    } catch (e) { if(activeSong.current===s.id)setError((e as Error).message); }
  };
  useEffect(() => {
    let current=true;
    if (p?.songId && p.songId !== song?.id) {
      activeSong.current=p.songId;
      setSelectedFingerNotes(null);
      void songApi<Song>(`/${p.songId}`)
        .then(s=>{if(current && activeSong.current===s.id)setSong(previous=>previous?.id===s.id && (previous.fingeringRevision ?? 0)>(s.fingeringRevision ?? 0) ? previous : s);})
        .catch((e) => {if(current)setError(e.message);});
    }
    return()=>{current=false;};
  }, [p?.songId]);
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
      .catch((e) => {if(current)setError(e.message);});
    return () => {
      current = false;
    };
  }, [song?.id, song?.fingeringRevision, bucket, p?.revision]);
  const refreshFingerings = async (revision: number) => {
    const id=song?.id;
    if(!id)return;
    try {
      const updated=await songApi<Song>(`/${id}`);
      if(activeSong.current===id && (updated.fingeringRevision ?? 0)>=revision)
        setSong(previous=>previous?.id===id && (previous.fingeringRevision ?? 0)<=(updated.fingeringRevision ?? 0) ? updated : previous);
    } catch(e){if(activeSong.current===id)setError((e as Error).message);}
  };
  const config = (edit: Partial<TransportConfig>) =>
    void run("transport.configure", edit);
  const togglePassage = async (hide: boolean) => {
    if (!hide) { setHidePassage(false); return; }
    setHidingPassage(true);
    setError("");
    try {
      await native.command("transport.configure", { loop: false });
      setHidePassage(true);
    } catch (e) { setError((e as Error).message); }
    finally { setHidingPassage(false); }
  };
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
  const selection: LoopSelection | undefined = song && p ? {
    a: p.config.a, b: p.config.b, duration: song.durationTicks, enabled: p.config.loop,
    start: () => run("transport.pause"),
    change: (a, b) => run("transport.configure", { a, b }),
  } : undefined;
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
            {song.tutorial && <section aria-label="Lesson guide" className="lesson-guide">
              <details key={song.id} open>
                <summary>Lesson guide · {song.title}</summary>
                <p className="lesson-badge">{song.tutorial.level} · {song.tutorial.collection}</p>
                <p>{song.tutorial.goal}</p>
                <p><strong>Suggested starting tempo: {song.tutorial.startTempo} quarter-note BPM.</strong> Use the tempo control below; slow further if needed.</p>
                <ol>{song.tutorial.steps.map((step,i) => <li key={i}>{step}</li>)}</ol>
                <ul>{song.tutorial.tips.map((tip,i) => <li key={i}>{tip}</li>)}</ul>
                <a href={song.tutorial.sourceUrl} target="_blank" rel="noreferrer">Source / score</a>
                <small>Guides suggest a practice approach. MIDI scoring checks pitch and timing; it does not judge fingering, tone or physical technique.</small>
              </details>
            </section>}
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
            <p className={`retry song-status${p.waiting || p.retry ? "" : " inactive"}`} role="status" aria-hidden={!p.waiting && !p.retry}>
              {p.waiting ? <>
                Waiting for the selected hand{p.config.hand === "both" ? "s" : ""}: press {p.target.join(" + ")} to continue.
              </> : p.retry ? <>
                Paused on miss — play a fresh target chord:{" "}
                {p.target.join(" + ")}. Backing resumes after your answer.
              </> : null}
            </p>
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
              {!hidePassage && <div className="song-options">
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
              </div>}
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
              <div className="player-tools-grid">
              <section className="player-tool-section" aria-label="Fingering controls">
                <div className="player-tool-heading">
                  <strong>Fingerings</strong>
                  <label><input type="checkbox" checked={hideFingerings}
                    onChange={e => { setHideFingerings(e.target.checked); if (e.target.checked) setSelectedFingerNotes(null); }} aria-label="Hide fingerings" />Hide</label>
                </div>
                <div className="player-tool-body" hidden={hideFingerings}>
              <FingeringControls key={song.id} songId={song.id} revision={song.fingeringRevision ?? 0}
                mode={native.state?.preferences.fingeringTips ?? "medium"}
                onModeChange={fingeringTips=>void run("preferences.configure",{fingeringTips})}
                onSaved={revision=>void refreshFingerings(revision)} selectedNoteIds={selectedFingerNotes}
                onCloseEditor={()=>setSelectedFingerNotes(null)} />
              <button aria-pressed={selectedFingerNotes!==null} onClick={()=>setSelectedFingerNotes(selectedFingerNotes===null ? [] : null)}>Edit fingering</button>
                </div>
              </section>
              <section className="player-tool-section" aria-label="Passage controls">
              <div className="player-tool-heading">
                <strong>Passage controls</strong>
                <label><input type="checkbox" aria-label="Hide passage controls"
                  checked={hidePassage} disabled={hidingPassage}
                  onChange={e => void togglePassage(e.target.checked)} />
                  Hide</label>
              </div>
              {!hidePassage && <>
              <div className="selection-controls">
                <label><input type="checkbox" aria-label="Repeat selected part"
                  checked={p.config.loop} onChange={e => config({ loop: e.target.checked })} />
                  Repeat selected part</label>
                <button onClick={() => config({ a: Math.max(0, Math.min(p.config.b - 1, Math.floor(p.songTick))) })}>Set start here</button>
                <button onClick={() => config({ b: Math.min(song.durationTicks, Math.max(p.config.a + 1, Math.floor(p.songTick))) })}>Set end here</button>
                <button onClick={() => config({ loop: false, a: 0, b: song.durationTicks })}>Clear selection</button>
                <button onClick={() => { setSeekPreview(null); void run("transport.return"); }}>Return to Start</button>
                <small>Drag Start / End on the score or overview. Drag the score to find another passage.</small>
              </div>
              <div ref={overview} className="selection-overview" aria-label="Whole piece selection overview">
                <LoopMarkers selection={selection!}
                  position={tick => tick / song.durationTicks * (overview.current?.clientWidth ?? 0)}
                  tickAt={x => {
                    const box = overview.current!.getBoundingClientRect();
                    return (x - box.left) / box.width * song.durationTicks;
                  }} />
              </div>
              </>}
              </section>
              </div>
              <PlayerControls
                hand={p.config.hand ?? "both"}
                onHand={(hand) => config({ hand })}
                playing={p.state === "playing" || p.waiting || p.state === "returning"}
                playLabel={p.state === "playing" || p.waiting || p.state === "returning" ? "Pause" : "Play"}
                onPlay={() =>
                  void run(
                    p.state === "playing" || p.waiting || p.state === "returning"
                      ? "transport.pause"
                      : "transport.play",
                  )
                }
              />
              <div className="practice-score">
                <EngravedScore
                  key={song.id}
                  fingeringMode={native.state?.preferences.fingeringTips ?? "medium"}
                  editingFingering={selectedFingerNotes!==null}
                  onFingeringNote={setSelectedFingerNotes}
                  song={song}
                  selection={hidePassage ? undefined : selection}
                  fromTick={scoreWindow.from}
                  toTick={scoreWindow.to}
                  notes={displayedNotes}
                  sampleRate={p.sampleRate}
                  clockFrame={() =>
                    p.return ? returnPosition(p.return,native.frame())*p.sampleRate :
                    (seekPreview ?? p.songSeconds) * p.sampleRate +
                    (p.state === "playing"
                      ? Math.max(0, native.frame() - p.sampleFrame) * p.speed
                      : 0)
                  }
                  pixelsPerSecond={p.config.spacing}
                  running={p.state === "playing" || p.state === "returning"}
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
