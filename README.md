# Sight Reading — local piano studio

Windows 11 x64 native C++ piano service with a self-hosted React interface. MIDI monitoring, sampled piano and metronome belong to the native process and continue across tabs, dialogs, fullscreen, reload and browser closure. No browser sound fallback or runtime CDN dependency.

This stage provides the global piano/metronome, local API, durable settings, Staff practice and five-route shell. Ear/Flash exercises and MIDI import/playback are subsequent tasks; their route workspaces identify that boundary.

## Reproducible setup

Use PowerShell from this project directory. Node 22.16.0 / npm 11.4.1 builds/tests the frontend; it is unnecessary at runtime. Portable compiler tools and samples are documented in [native/README.md](native/README.md).

```powershell
pwsh scripts/setup-native.ps1 -DownloadTools
pwsh scripts/setup-service.ps1
npm --prefix web ci --cache .tools/npm-cache
pwsh scripts/build-native.ps1 -Tests
npm --prefix web run build
pwsh scripts/start.ps1
```

Native source revisions/checksums are pinned in `native/dependencies.json`, `native/toolchain.json` and `native/service-dependencies.json`; frontend exact versions/transitive resolution are in `web/package-lock.json`. Tools, caches, dependencies, build output and downloaded samples stay out of Git. Runtime requires the native executable, LLVM-MinGW runtime DLLs, `web/dist`, and `assets/piano`. `setup-service.ps1 -Offline` verifies cached sources. Final portable release packaging is a later task.

## Start, data and stop

Default address: http://127.0.0.1:8765. `scripts/start.ps1 -Port 8766 -DataDir path -NoBrowser` supports isolated instances. Or run `build/native/native/sightreading.exe --port 8765 --data-dir path --web-dir web/dist` from the project root, with the compiler `bin` directory on PATH. The launcher starts hidden and prints the process PID; `Stop-Process -Id <PID>` stops it. Foreground service accepts Ctrl+C. Browser closure intentionally keeps native hardware and metronome running; tray/lifecycle release polish is a later task.

Persistent settings/library schema: `%LOCALAPPDATA%/SightReading/practice.sqlite` or explicit data-dir. Preserve that directory across upgrades. Back it up while native service is stopped, including any WAL files. Metronome settings persist; native-process restart starts it stopped, browser reload preserves running state.

## Global controls

Audio/MIDI, monitoring, master volume, metronome and Panic stay above route content/dialogs. M opens metronome settings unless typing in a field. Fullscreen targets the document so configuration stays accessible. Device/exclusive changes defer until live keys and sustain release; Apply now intentionally interrupts. Ordinary monitoring, output-mode, external-port and volume changes do not rebuild WASAPI.

Tempo is quarter-note BPM (20–300). Meter uses denominator beats: 6/8 means six eighth beats. Subdivision values mean 1, 2, 4 or 3 pulses per denominator beat. Tempo edits apply next beat; meter/divisions/swing next bar; count-in edits on next start. Follow continues at the last tempo after transport stop. Only explicit click Stop, Panic or native shutdown stops clicks.

Native loading, missing samples/audio, overload and deferred changes display accurately. Output panel reports actual format and API output latency; this is not measured MIDI-to-audible latency.

The global virtual piano covers A0–C8 (88 keys), with C4=MIDI60. Use mouse/touch or computer keys A W S E D F T G Y H U J K O L P ; for a chromatic range starting at the selected C octave. Space holds virtual sustain; M stays reserved for metronome settings. Keyboard settings change virtual attack velocity and labels. Full range pans horizontally on narrow screens; focused range and octave controls are independent of exercise range. Browser focus loss/cancel releases its virtual ownership only. Live/MIDI, playback and example notes have separate colors; hatching indicates sustained notes.

Staff has native seeded generation and physical-key scoring in Treble/Bass/Grand/chord-symbol modes. Open Staff settings for random 1–5 notes, one/two hands, chord-based targets, triad inversions, open seventh voicings, Autumn Leaves/50s/circle/tritone-substitution progressions, five-finger drills and selected diatonic intervals. Five-finger drills keep one hand position; Starting finger chooses the first pitch and later targets show the corresponding finger. All named keys and Chromatic are available, with major/natural/harmonic/ascending melodic minor scales. Chord symbols offer ordered/random chords, multiple keys, 3/4 tones and common pitch classes through the displayed note/MIDI threshold.

Wait requires the exact physically held pitch set and fresh attacks within 150 ms; wrong attacks count misses and permit retry after release. Sustain and examples never answer. Scroll uses native quarter-note tempo and adjustable timing tolerance (default ±150 ms). Each seeded session has 64 targets; Start/Reset returns to target one. Reset clears score/timing, retains held live notes and requires fresh attacks. Stop/settings edits affect this exercise only; live monitoring and global clicks continue. Invalid combinations are rejected atomically with an actionable message. Smoothness 0 is unrestricted; 1–6 reduces allowed target-center motion from 12 to 2 semitones. Hand groups span at most one octave. Numeric settings commit on blur/Enter; checkboxes/selects follow authoritative native snapshots.

The reusable VexFlow 5.0.0 renderer uses bundled Bravura/Academico fonts (offline, no CDN), joined Grand clefs, native pitch spelling separate from MIDI, accidentals/ledger lines/duration bars and native-clock visual scrolling. Segment SVGs allow glyph overflow so the current target at NOW remains intact. VexFlow is pinned in the lockfile; its rendering API follows the [official tutorial](https://github.com/vexflow/vexflow/wiki/Tutorial).

## Verification

```powershell
pwsh scripts/build-native.ps1 -Tests
npm --prefix web run typecheck
npm --prefix web test
npm --prefix web run build
# Prepend .tools/llvm-mingw-20261006-ucrt-x86_64/bin to PATH:
node tests/service.mjs
node tests/browser.mjs
node tests/keyboard-notation.mjs
node tests/staff-practice.mjs
```

API/browser tests use temporary databases and actual native executable. Browser tests use installed Edge with fresh headless contexts, no browser download. Restricted environments may need loopback socket access permission.

Physical MIDI hotplug/latency, actual device/exclusive rebuilds and 30-minute dropout soak remain unvalidated. An immediate WASAPI reopen failed on this machine; ordinary mode/monitor changes avoid unnecessary reopen. Genuine output-device changes still expose native failure and need hardware validation.

## Attribution

Salamander Grand Piano: Alexander Holm, CC BY 3.0, compatible mapping attributed to kinwie; see `assets/piano` and `native/README.md`. Application click waveforms are original. cpp-httplib MIT; SQLite public domain. VexFlow MIT; bundled Bravura and Academico fonts SIL OFL. React/Vite/TypeScript/Vitest/Playwright use redistributable upstream licenses. No code or assets copied from the UNLICENSED reference.
