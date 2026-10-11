# Sight Reading — local piano studio

Windows 11 x64 native C++ piano service with a self-hosted React interface. MIDI monitoring, sampled piano and metronome belong to the native process and continue across tabs, dialogs, fullscreen, reload and browser closure. No browser sound fallback or runtime CDN dependency.

This stage provides the global piano/metronome, local API, durable settings and five-route shell. Exercise bodies and MIDI import/playback are subsequent tasks; route workspaces identify that boundary.

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

## Verification

```powershell
pwsh scripts/build-native.ps1 -Tests
npm --prefix web run typecheck
npm --prefix web test
npm --prefix web run build
# Prepend .tools/llvm-mingw-20261006-ucrt-x86_64/bin to PATH:
node tests/service.mjs
node tests/browser.mjs
```

API/browser tests use temporary databases and actual native executable. Browser tests use installed Edge with fresh headless contexts, no browser download. Restricted environments may need loopback socket access permission.

Physical MIDI hotplug/latency, actual device/exclusive rebuilds and 30-minute dropout soak remain unvalidated. An immediate WASAPI reopen failed on this machine; ordinary mode/monitor changes avoid unnecessary reopen. Genuine output-device changes still expose native failure and need hardware validation.

## Attribution

Salamander Grand Piano: Alexander Holm, CC BY 3.0, compatible mapping attributed to kinwie; see `assets/piano` and `native/README.md`. Application click waveforms are original. cpp-httplib MIT; SQLite public domain. React/Vite/TypeScript/Vitest/Playwright use redistributable upstream licenses. No code or assets copied from the UNLICENSED reference.
