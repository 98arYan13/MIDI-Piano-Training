# Tutorial library verification — 2026-10-13

Added 83 guided MIDI lessons: 16 independently authored Foundations, 18 public-domain Mutopia Burgmüller Op. 100 editions, 10 Czerny Op. 840 and 19 Op. 821 editions, and 20 attributed CC BY-SA 4.0 Hanon Part I exercises. Catalog entries record source URLs, licenses, edition attribution, per-file SHA-256, lesson levels, suggested tempos, goals and practice steps. Downloaded/generated tutorial MIDI is ignored by Git and recreated by `node scripts/setup-tutorials.mjs`.

Play Along now provides title/skill search, difficulty/collection filters and a collapsible guide for each selected lesson. Native C++ still imports, stores, scores and plays all MIDI. Installation adds available tutorial IDs without replacing existing originals, assignments, renamed titles or recoverable trash. Packaging requires every pinned tutorial file and includes attribution plus the tutorial/user guides.

## Results

- Native Release build and all four CTest targets passed: protection, piano audio render, storage and transport audio render.
- All 34 frontend tests across eight files passed; TypeScript and Vite production build passed. Existing Vite bundle-size and upstream CMake warnings remain.
- Full maintained integration runner passed all eleven suites: service, browser, keyboard/notation, Staff, Play Along, dragging/wait/hints, Ear/Flash, final notation, dense score, focus mode and tutorial library.
- New tutorial regression imports all 83 files through native startup, compares each downloaded original byte-for-byte, loads every tutorial into native transport and verifies nonempty event windows. Browser checks search, difficulty, source links, per-lesson guide and mobile display. Play/pause/seek retains a held virtual test note and running metronome.
- Setup verified all 83 checksums offline. The content worker also verified deterministic regeneration of a removed original lesson file.
- Independent review found no high/medium issues. Existing tutorial rows deliberately retain their initially imported metadata on later catalog updates; song edits and trash are preserved.
- Visual evidence: `tutorial-library-desktop.png` and `tutorial-library-mobile.png`. Guides can be collapsed or hidden using the existing Focus mode to bring the score forward.
- Updated local service on port 8765 exposes 83 guided lessons and 95 total songs: ten original starters plus two retained user imports. It was refreshed only after checking that no notes were held, the metronome was stopped and transport was paused.

## Limits

Difficulty, goals and starting tempos are independently authored practice recommendations, not composer or examination-board annotations. The external edition is authoritative for fingering, dynamics and articulation. Guides and MIDI work offline after setup; source/score links require internet. No physical MIDI keyboard, technique assessment or measured hardware latency certification was performed in this change. Automated ownership verification uses virtual native input.
