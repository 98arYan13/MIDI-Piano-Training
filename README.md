# Sight Reading — local piano studio

Windows 11 x64 portable native C++ piano studio with five complete rooms: Staff, Ear Training, Flash Cards, Play Along and Guide. The native service owns MIDI, sampled audio, metronome, playback clock, imports, scoring and persistence. The browser renders an offline interface. Hardware monitoring and clicks continue after browser closure.

## Use the portable release

Copy the entire `build/release/SightReading` folder to a writable local folder and run `Start.cmd`. Open http://127.0.0.1:8765. No Node, compiler PATH, internet, account or system-wide installation is required. Keep the executable, its two runtime DLLs, web assets and piano samples together. The Windows notification-area icon offers **Open Sight Reading** and **Quit and stop sound**; `Stop.cmd` also shuts down gracefully. Closing a browser is not shutdown.

For another port/data directory: `powershell -File scripts/start.ps1 -Port 8766 -DataDir "D:\PianoData" -NoBrowser`; stop with `powershell -File scripts/stop.ps1 -Port 8766`. The direct executable accepts `--port`, `--data-dir`, `--web-dir` and `--no-tray`. Launch from the application folder so relative sample paths resolve.

Persistent settings, imported originals, song assignments, recent items and recoverable trash are separate from the release in `%LOCALAPPDATA%\SightReading`, or the explicit data directory. Quit before backing up the complete directory, including SQLite/WAL and originals. Replacing/removing the app folder preserves that data. Restore the directory before starting an upgraded app. Process restart begins stopped; browser reload reconnects to current native state.

See [the complete user guide](docs/USER-GUIDE.md) for every room, global controls, shortcuts, imports, loops, scoring, tuning and troubleshooting. `M` opens metronome settings outside typing fields. Audio/MIDI and fullscreen remain accessible above route settings. Panic explicitly clears all buses and clicks. Device rebuilds defer while live keys or sustain remain.

## Run again after the initial build

Once the native service and web frontend have been built, run these commands in PowerShell; no setup or rebuild is needed:

```powershell
cd "C:\Users\Amin\Documents\ChatGPT\Sight Reading"
.\scripts\start.ps1
```

The launcher starts the native engine and opens http://127.0.0.1:8765 in your browser. If your checkout is elsewhere, use its folder in the `cd` command.

To stop the engine from the same project folder:

```powershell
.\scripts\stop.ps1
```

You can also quit through the notification-area icon. Closing the browser leaves the engine running. For the packaged portable release, double-click `build\release\SightReading\Start.cmd`; use `Stop.cmd` in that folder to stop it.

## Reproducible developer setup

Run from this checkout in PowerShell:

```powershell
pwsh scripts/setup-native.ps1 -DownloadTools
pwsh scripts/setup-service.ps1
npm.cmd --prefix web ci --cache .tools/npm-cache
pwsh scripts/build-native.ps1 -Tests
npm.cmd --prefix web run build
pwsh scripts/package-release.ps1
pwsh scripts/start.ps1
```

Build tools are project-local LLVM-MinGW 20261006 (Clang 23), CMake 4.4.4 and Ninja 1.13.2. Node 22.16.0/npm 11.4.1 are build/test tools only. Exact source pins/checksums: `native/dependencies.json`, `native/toolchain.json`, `native/service-dependencies.json`, `web/package-lock.json`, and piano/music manifests. `setup-service.ps1 -Offline` verifies cached sources. Downloads, caches, dependencies, samples and builds are excluded from Git. No reference-project code/assets are used; its UNLICENSED source was not copied.

The package script requires a fresh destination beneath `build/`, bundles actual `libc++.dll`/`libunwind.dll` PE runtime imports and writes SHA-256/size provenance in `release-manifest.json`. Windows/UCRT system DLLs are supplied by Windows. `scripts/update-notices.ps1` collects checked-in upstream notices from pinned cached sources; no compiler or test-only dependencies ship.

## Verification

```powershell
pwsh scripts/build-native.ps1 -Tests
npm.cmd --prefix web ci --cache .tools/npm-cache
npm.cmd --prefix web run typecheck
npm.cmd --prefix web test
npm.cmd --prefix web run test:e2e
npm.cmd --prefix web run build
```

`test:e2e` runs maintained service, browser, keyboard/notation, Staff, Play Along, Ear/Flash and final notation regression suites with isolated databases and installed headless Edge/Chrome. Set `SR_BROWSER` to an explicit browser executable if needed. It fails fast and never downloads a browser. The native SQLite experimental notice is suppressed only for these Node integration processes; error warnings remain enabled. Release/performance and 30-minute actual WASAPI tooling are `tests/release.mjs` and `tests/soak.mjs`; see [release verification](docs/RELEASE-VERIFICATION.md) for exact commands, machine, evidence and limitations.

## Runtime limits and attribution

48 kHz/128-frame shared WASAPI is requested; the UI reports actual negotiated values and API output latency, distinct from measured MIDI-to-audible latency. Live sampler layers have 1024 protected voices (960 attack +64 release reserve), refuse new allocations instead of stealing, and are isolated from Playback/Example/Click. SMF0/1 PPQ imports retain originals, tempo/meter/key/channel/controller data; limits are 20 MB/one million events/256 tracks/two hours/INT32_MAX cumulative ticks. Type2/SMPTE are rejected. CC64/7/11/10/120/123 audition is supported; other controllers and pitch bend are retained without an audition mapping. Dense notation caps 4096 notes/window with an explicit truncation message; accompaniment/restore bounds are documented in [native/CONTRACT.md](native/CONTRACT.md).

Salamander Grand Piano: Alexander Holm, CC BY 3.0; mapping informed by kinwie's reconstruction, unchanged samples. Five original simplified public-domain starter arrangements and original CC0 interval motifs are attributed in `assets/music/LICENSE.md` and `assets/ear/README.md`. Click waveforms are original. All runtime dependencies and embedded Bravura/Academico font notices are in [third-party notices](docs/licenses/THIRD-PARTY-NOTICES.txt). Hardware MIDI hotplug, external instrument/wheel, wired arrival-to-audible p95 and speaker-level dropout measurements require actual equipment and are reported separately from automated native audio evidence.
