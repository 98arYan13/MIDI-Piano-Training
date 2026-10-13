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
node scripts/setup-tutorials.mjs
npm.cmd --prefix web ci --cache .tools/npm-cache
pwsh scripts/build-native.ps1 -Tests
npm.cmd --prefix web run build
pwsh scripts/package-release.ps1
pwsh scripts/start.ps1
```

Build tools are project-local LLVM-MinGW 20261006 (Clang 23), CMake 4.4.4 and Ninja 1.13.2. Node 22.16.0/npm 11.4.1 are build/test tools only. Exact source pins/checksums: `native/dependencies.json`, `native/toolchain.json`, `native/service-dependencies.json`, `web/package-lock.json`, and piano/music manifests. `setup-service.ps1 -Offline` verifies cached sources. Downloads, caches, dependencies, samples and builds are excluded from Git. No reference-project code/assets are used; its UNLICENSED source was not copied.

Automatic Play Along fingering uses the attributed, pretrained ArGNN research checkpoints. Provision the project-local CPython 3.11.9 / CPU PyTorch 2.8.0 runtime and real left/right weights with `pwsh scripts/setup-fingering.ps1`; after the first complete setup, `pwsh scripts/setup-fingering.ps1 -Offline` verifies the same cached archives and installs without network access. If the configured PyPI host is unavailable, `-PinnedMirror` selects the reviewed Tsinghua PyPI mirror; the same exact wheel hashes are still required. The runtime, wheels and weights stay under ignored `.tools/fingering/`. Exact revisions and SHA-256 pins are in `fingering/dependencies.json` and `fingering/requirements.lock`; see [third-party notices](docs/licenses/THIRD-PARTY-NOTICES.txt). Fingering remains unavailable with a clear setup error when either checkpoint is missing or corrupt; no substitute model is used.

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

Play Along also has guided MIDI lessons: original foundations, Burgmüller Op. 100, Czerny Op. 840/821 and Hanon Part I. Search by title or skill and filter by difficulty or collection; choosing a tutorial shows its goal, starting tempo, practice steps, tips and source/score link. Provision the checksum-pinned files with `node scripts/setup-tutorials.mjs` before tutorial tests or packaging. Downloaded MIDI stays in ignored `assets/music/tutorials/`; the catalog, original lesson definitions and reproducible setup stay in Git. Startup adds available lessons to existing libraries without replacing imports, edits or trash. Once provisioned, playback and guides work offline; external score links need internet. See [tutorial collection and practice guidance](docs/TUTORIALS.md).

## Runtime limits and attribution

48 kHz/128-frame shared WASAPI is requested; the UI reports actual negotiated values and API output latency, distinct from measured MIDI-to-audible latency. Live sampler layers have 1024 protected voices (960 attack +64 release reserve), refuse new allocations instead of stealing, and are isolated from Playback/Example/Click. SMF0/1 PPQ imports retain originals, tempo/meter/key/channel/controller data; limits are 20 MB/one million events/256 tracks/two hours/INT32_MAX cumulative ticks. Type2/SMPTE are rejected. CC64/7/11/10/120/123 audition is supported; other controllers and pitch bend are retained without an audition mapping. Dense notation caps 4096 notes/window with an explicit truncation message; accompaniment/restore bounds are documented in [native/CONTRACT.md](native/CONTRACT.md).

Salamander Grand Piano: Alexander Holm, CC BY 3.0; mapping informed by kinwie's reconstruction, unchanged samples. Ten built-in MIDI pieces (five simplified public-domain starter arrangements and five original practice exercises) and original CC0 interval motifs are attributed in `assets/music/LICENSE.md` and `assets/ear/README.md`. Click waveforms are original. All runtime dependencies and embedded Bravura/Academico font notices are in [third-party notices](docs/licenses/THIRD-PARTY-NOTICES.txt). Hardware MIDI hotplug, external instrument/wheel, wired arrival-to-audible p95 and speaker-level dropout measurements require actual equipment and are reported separately from automated native audio evidence.

Portable fingering releases include embedded Python, the exact CPU inference dependencies, both checkpoints and attribution. Runtime generation needs no system Python, pip, Node, compiler, GPU or network. Packaging verifies cached archive/checkpoint SHA-256 pins and assembles their contents without installers or training datasets. Run `pwsh scripts/setup-fingering.ps1 -Offline` after provisioning, then `pwsh scripts/update-notices.ps1` and `pwsh scripts/package-release.ps1 -Destination build/release/SightReading-new`. The optional `-NativeExecutable build/fingering-native/native/sightreading.exe` selects an already verified alternate build without replacing a running executable. Choose a fresh destination each time. See [fingering verification](docs/verification/2026-10-13-fingering.md).

For isolated integration checks set `SR_FINGERING_EXECUTABLE` to the verified executable and point process-local TMP/TEMP at a writable project test directory. The fingering integration suites are part of the normal browser runner. `SR_RELEASE` selects the exact portable folder for `node tests/release.mjs`; its child uses System32-only PATH and a disposable data directory.

The CPU Torch runtime imports Microsoft MSVCP140/VCRUNTIME140/VCRUNTIME140_1. Portable setup requires applicable Visual Studio license rights for unchanged app-local redistribution; the redistributable end-user license alone does not grant sharing rights. After confirming those rights, use `pwsh scripts/setup-fingering.ps1 -Offline -ConfirmRedistributionRights` (omit Offline for first provisioning) and pass `-ConfirmRedistributionRights` to package-release. This extracts the exact pinned official archive through Windows cabinet extraction; it never runs the installer or changes global settings. Both commands fail closed without explicit confirmation or verified local DLLs. The original license and exact DLL hashes ship with a completed package. See [Microsoft redistribution terms](https://learn.microsoft.com/en-us/visualstudio/releases/2022/redistribution).
