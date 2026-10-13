import { useEffect, useState } from "react";
import { songApi } from "./songApi";
import type { Song } from "./types";

const FAVORITES_KEY = "sight-reading.favorite-songs.v1";

function loadFavorites(): string[] {
  try {
    const value: unknown = JSON.parse(localStorage.getItem(FAVORITES_KEY) ?? "[]");
    return Array.isArray(value) ? value.filter((id): id is string => typeof id === "string") : [];
  } catch {
    return [];
  }
}
export function Library({
  selected,
  choose,
}: {
  selected: string;
  choose: (song: Song) => void;
}) {
  const [songs, setSongs] = useState<Song[]>([]),
    [trash, setTrash] = useState<Song[]>([]),
    [error, setError] = useState(""),
    [busy, setBusy] = useState(false),
    [showTrash, setShowTrash] = useState(false),
    [search, setSearch] = useState(""),
    [favorites, setFavorites] = useState<string[]>(loadFavorites),
    [favoritesOnly, setFavoritesOnly] = useState(false),
    [level, setLevel] = useState("All"),
    [collection, setCollection] = useState("All");
  const filtered = songs.filter(s =>
    (!favoritesOnly || favorites.includes(s.id)) &&
    (level === "All" || s.tutorial?.level === level) &&
    (collection === "All" || s.tutorial?.collection === collection) &&
    `${s.title} ${s.tutorial?.collection ?? ""} ${s.tutorial?.goal ?? ""}`.toLowerCase().includes(search.trim().toLowerCase())
  ).sort((a,b) => (a.tutorial?.order ?? 0) - (b.tutorial?.order ?? 0) || a.title.localeCompare(b.title));
  const toggleFavorite = (id: string) => {
    const next = favorites.includes(id) ? favorites.filter(item => item !== id) : [...favorites, id];
    setFavorites(next);
    try { localStorage.setItem(FAVORITES_KEY, JSON.stringify(next)); } catch { /* Keep this session's preference if storage is unavailable. */ }
  };
  const refresh = async () => {
    const [items, removed] = await Promise.all([
      songApi<Song[]>(""),
      songApi<Song[]>("/trash"),
    ]);
    setSongs(items);
    setTrash(removed);
  };
  useEffect(() => {
    void refresh().catch((e) => setError(e.message));
  }, []);
  const action = async (fn: () => Promise<unknown>) => {
    setError("");
    setBusy(true);
    try {
      await fn();
      await refresh();
    } catch (e) {
      setError((e as Error).message);
    } finally {
      setBusy(false);
    }
  };
  const upload = (file?: File) => {
    if (!file) return;
    void action(async () => {
      const form = new FormData();
      form.append("file", file);
      const song = await songApi<Song>("/import", {
        method: "POST",
        body: form,
      });
      choose(song);
    });
  };
  return (
    <aside
      className="song-library"
      onDragOver={(e) => {
        e.preventDefault();
      }}
      onDrop={(e) => {
        e.preventDefault();
        upload(e.dataTransfer.files[0]);
      }}
    >
      <header>
        <h2>Your MIDI library</h2>
        <span>{songs.length} songs</span>
      </header>
      <label className="upload">
        Upload .mid / .midi
        <input
          type="file"
          aria-label="Upload MIDI"
          accept=".mid,.midi"
          disabled={busy}
          onChange={(e) => {
            upload(e.target.files?.[0]);
            e.target.value = "";
          }}
        />
      </label>
      <small>Or drop a file here · stored only on this computer</small>
      <div className="library-filters">
        <label>Search<input type="search" aria-label="Search MIDI library" placeholder="Title or skill" value={search} onChange={e => setSearch(e.target.value)} /></label>
        <label>Difficulty<select aria-label="Lesson difficulty" value={level} onChange={e => setLevel(e.target.value)}>
          {["All", "Beginner", "Elementary", "Intermediate"].map(v => <option key={v}>{v}</option>)}
        </select></label>
        <label>Collection<select aria-label="Lesson collection" value={collection} onChange={e => setCollection(e.target.value)}>
          <option>All</option>{[...new Set(songs.flatMap(s => s.tutorial ? [s.tutorial.collection] : []))].sort().map(v => <option key={v}>{v}</option>)}
        </select></label>
        <label className="favorites-filter"><input type="checkbox" aria-label="Show favorites only" checked={favoritesOnly} onChange={e => setFavoritesOnly(e.target.checked)} /> Favorites only</label>
        <small>{filtered.length} of {songs.length} pieces · {songs.filter(s => s.tutorial).length} guided lessons</small>
      </div>
      {error && <p role="alert">{error}</p>}
      <div className="song-list">
        {filtered.length === 0 && <p>No matching pieces. Try another search or difficulty.</p>}
        {filtered.map((s) => (
          <div
            className={`song-row ${selected === s.id ? "selected" : ""}`}
            key={s.id}
          >
            <button className="song-select" disabled={busy} onClick={() => choose(s)}>
              <strong>{s.title}</strong>
              {s.tutorial && <small className="lesson-badge">{s.tutorial.level} · {s.tutorial.collection}</small>}
              <small>
                {Math.round(s.durationSeconds)} sec · {s.tracks.length} tracks{" "}
                {s.recent > 0 ? "· recent" : ""}
              </small>
            </button>
            <button
              className={`song-favorite ${favorites.includes(s.id) ? "is-favorite" : ""}`}
              type="button"
              aria-label={`${favorites.includes(s.id) ? "Remove from" : "Add to"} favorites: ${s.title}`}
              aria-pressed={favorites.includes(s.id)}
              title={favorites.includes(s.id) ? "Remove from favorites" : "Add to favorites"}
              onClick={() => toggleFavorite(s.id)}
            >
              {favorites.includes(s.id) ? "★" : "☆"}
            </button>
            <a
              href={`/api/v1/songs/${encodeURIComponent(s.id)}/original`}
              download={`${s.title.replace(/[<>:"/\\|?*]/g, "_")}.mid`}
              aria-label={`Download MIDI: ${s.title}`}
            >
              Download MIDI
            </a>
            <button
              title={`Move ${s.title} to trash`}
              aria-label={`Trash ${s.title}`}
              disabled={busy}
              onClick={() =>
                void action(() => songApi(`/${s.id}`, { method: "DELETE" }))
              }
            >
              ×
            </button>
          </div>
        ))}
      </div>
      <button onClick={() => setShowTrash(!showTrash)}>
        Recoverable trash ({trash.length})
      </button>
      {showTrash &&
        trash.map((s) => (
          <div className="song-row" key={s.id}>
            <span>{s.title}</span>
            <button
              disabled={busy}
              onClick={() =>
                void action(() =>
                  songApi(`/${s.id}/restore`, { method: "POST" }),
                )
              }
            >
              Restore
            </button>
          </div>
        ))}
    </aside>
  );
}
