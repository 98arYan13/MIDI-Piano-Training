import { useEffect, useRef, useState } from "react";
import { songApi } from "./songApi";
import type { FingeringRun, JobSnapshot } from "./types";
import type { FingeringMode } from "./notationModel";
type Status = {
  job: JobSnapshot;
  revision: number;
  summary: FingeringRun | null;
};
export function FingeringControls({
  songId,
  revision,
  mode,
  onModeChange,
  onSaved,
  selectedNoteIds,
  onCloseEditor,
}: {
  songId: string;
  revision: number;
  mode: FingeringMode;
  onModeChange: (mode: FingeringMode) => void;
  onSaved: (revision: number) => void;
  selectedNoteIds: string[] | null;
  onCloseEditor: () => void;
}) {
  const [status, setStatus] = useState<Status | null>(null),
    [error, setError] = useState(""),
    [busy, setBusy] = useState(false);
  const epoch = useRef(0),
    saved = useRef(revision),
    knownJob = useRef("");
  const onSavedRef = useRef(onSaved);
  onSavedRef.current = onSaved;
  useEffect(() => {
    const generation = ++epoch.current;
    saved.current = revision;
    knownJob.current = "";
    setStatus(null);
    setError("");
    setBusy(false);
    const poll = async () => {
      try {
        const s = await songApi<Status>(`/${songId}/fingering`);
        if (epoch.current !== generation) return;
        // The service has one global job; never attribute another song's result to this score.
        const own = s.job.songId === songId;
        if (own && knownJob.current && knownJob.current !== s.job.jobId) return;
        if (own) knownJob.current = s.job.jobId;
        setStatus(s);
        if (s.revision > saved.current) {
          saved.current = s.revision;
          onSavedRef.current(s.revision);
        }
        if (own && s.job.state === "failed")
          setError(s.job.error?.message ?? "Fingering generation failed.");
        else setError("");
      } catch (e) {
        if (epoch.current === generation) setError((e as Error).message);
      }
    };
    void poll();
    return () => {
      ++epoch.current;
    };
  }, [songId, revision]);
  const request = async (path: string, body?: unknown, method = "POST") => {
    // A mutation supersedes initial/status reads already in flight.
    const generation = ++epoch.current;
    setBusy(true);
    setError("");
    try {
      const result = await songApi<Status | JobSnapshot>(
        `/${songId}/fingering${path}`,
        {
          method,
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify(body),
        },
      );
      if (epoch.current !== generation) return;
      if ("revision" in result) {
        saved.current = result.revision;
        onSavedRef.current(result.revision);
        onCloseEditor();
      } else {
        knownJob.current = result.jobId;
        setStatus((previous) => ({
          revision: previous?.revision ?? revision,
          summary: previous?.summary ?? null,
          job: result,
        }));
      }
    } catch (e) {
      if (epoch.current === generation) setError((e as Error).message);
    } finally {
      if (epoch.current === generation) {
        setBusy(false);
        setStatus((s) => (s ? { ...s } : s));
      }
    }
  };
  const job = status?.job,
    running = job?.state === "running",
    own = job?.songId === songId;
  // Start a bounded poll after the Generate response; the initial GET may have been idle.
  useEffect(() => {
    if (!running) return;
    const generation = epoch.current;
    const timer = setTimeout(async () => {
      try {
        const s = await songApi<Status>(`/${songId}/fingering`);
        if (
          epoch.current !== generation ||
          (s.job.jobId !== knownJob.current && s.job.songId === songId)
        )
          return;
        setStatus(s);
        if (s.revision > saved.current) {
          saved.current = s.revision;
          onSavedRef.current(s.revision);
        }
        if (s.job.songId === songId && s.job.state === "failed")
          setError(s.job.error?.message ?? "Fingering generation failed.");
        else setError("");
      } catch (e) {
        if (epoch.current === generation) {
          setError((e as Error).message);
          setStatus((s) => (s ? { ...s } : s));
        }
      }
    }, 500);
    return () => clearTimeout(timer);
  }, [status, songId, running]);
  const ids =
    selectedNoteIds?.map((id) => (/^\d+$/.test(id) ? Number(id) : NaN)) ?? [];
  const valid =
    ids.length > 0 &&
    ids.every((id) => Number.isSafeInteger(id) && id > 0) &&
    new Set(ids).size === ids.length;
  return (
    <section className="fingering-controls" aria-label="Fingering">
      <button
        disabled={busy || running}
        onClick={() => void request("/generate")}
      >
        {status?.summary ? "Regenerate fingerings" : "Generate fingerings"}
      </button>
      <label>
        Fingering tips{" "}
        <select
          aria-label="Fingering tips"
          value={mode}
          onChange={(e) => onModeChange(e.target.value as FingeringMode)}
        >
          <option value="full">Full</option>
          <option value="medium">Medium</option>
          <option value="pro">Pro</option>
        </select>
      </label>
      {running && (
        <>
          <span role="status">
            {own
              ? `${job.stage || "Generating"}: ${job.completed} / ${job.total}`
              : "Generating fingerings for another song"}
          </span>
          {own && (
            <button
              disabled={busy}
              onClick={() => void request("/cancel", { jobId: job.jobId })}
            >
              Cancel fingering generation
            </button>
          )}
        </>
      )}
      {status?.summary && (
        <small>
          {status.summary.assigned} assigned · {status.summary.unresolved}{" "}
          unresolved · {status.summary.unsupported} unsupported
        </small>
      )}
      {selectedNoteIds !== null && (
        <div
          className="fingering-editor"
          role="group"
          aria-label="Manual fingering correction"
        >
          <span>
            {valid
              ? `Selected ${ids.length} note${ids.length === 1 ? "" : "s"}`
              : "Select a note on the score"}
          </span>
          {[1, 2, 3, 4, 5].map((finger) => (
            <button
              key={finger}
              disabled={!valid || busy}
              onClick={() =>
                void request("", { noteIds: ids, finger }, "PATCH")
              }
            >
              Finger {finger}
            </button>
          ))}
          <button
            disabled={!valid || busy}
            onClick={() =>
              void request("", { noteIds: ids, finger: null }, "PATCH")
            }
          >
            Use automatic
          </button>
          <button onClick={onCloseEditor}>Close fingering editor</button>
        </div>
      )}
      {error && <p role="alert">{error}</p>}
    </section>
  );
}
