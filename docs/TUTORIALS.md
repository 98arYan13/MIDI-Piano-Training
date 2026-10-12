# Tutorial library

Run from the repository root with Node.js 22 or later and Windows PowerShell available:

```powershell
node scripts/setup-tutorials.mjs
```

The setup installs 83 lessons into the ignored `assets/music/tutorials/` directory: 16 original beginner Foundations, 18 Burgmüller Op. 100 studies, 10 Czerny Op. 840 studies, 19 Czerny Op. 821 studies and 20 Hanon Part I exercises. Existing starter music and `manifest.json` are unchanged. Restart the native service after installation so it imports the additional catalog.

The checked-in `assets/music/tutorials.json` is both the catalog and checksum lock. The setup verifies existing files, generates original lessons deterministically, downloads only missing or mismatched external files, verifies every downloaded SHA-256, and verifies the Hanon archive before extracting its named MIDI members. A source checksum change stops installation; review upstream before updating the lock. No external JavaScript packages are needed. Temporary archive extraction uses `.cache/tutorials-setup/`, removed after successful setup. Downloads require internet access; subsequent runs with verified files are offline.

Each lesson contains collection, suggested level, sequence order, goal, starting BPM, practice steps, tips and a source link. Difficulty and starting BPM are teaching recommendations rather than scholarly rankings. Begin with Foundations; then select a short study whose score you can read slowly. Czerny is marked Intermediate conservatively. Hanon is a technique collection, not a replacement for varied music or a complete piano curriculum.

Use Play Along to listen and identify the pulse, then work on a small phrase. Practice one hand before combining hands, and increase by 5 BPM after three accurate repetitions. The written source score remains authoritative for fingering, dynamics, articulation and intended tempo. MIDI playback and scoring do not assess physical technique or expressive interpretation. Keep Hanon sessions short and relaxed.

Source editions and attribution are recorded in `assets/music/LICENSE.md` and each entry's provenance. Original note events are stored in the lock so they can be recreated without a notation compiler. Native C++ performs MIDI import and audio playback; these lessons do not add browser audio.
