# Task 3 implementation report

Status: DONE_WITH_CONCERNS (hardware coverage limits below). Base: d8b12778e515a5f979fc6fa163e6d531c59272ac. Commit: 839ce164ddd145a97806b12eb772cecc157e2407.

## Implementation

- Native Service.cpp: single loopback HTTP/WebSocket listener, strict exact Host/Origin rejection before upgrade, versioned commands, 30Hz complete state, timestamped clock ping, bounded callback event rings/history and reconnect resync. Reusable per-connection virtual source identities preserve isolation without exhausting native slots.
- Storage.hpp/storage.cpp tests: native SQLite schema1 transactional migration, future-schema rejection, atomic settings statements and songs table/list foundations. Restores canonical audio/metronome and disabled MIDI identities before discovery/start. Songs import/CRUD and exercise bodies belong to later tasks.
- Engine/NativeIO/Commands: preserve unchanged external output ports; rebuild WASAPI only for real audio-device/exclusive changes, preserving held deferral. Apply now uses audio-consumer scoped live reset with reserved release/recovery handling; keeps metronome, unrelated buses and queued unrelated events. Atomic native meter/accent/channel convenience edits prevent stale browser snapshots losing rapid compound edits.
- React provider/client outside routes: persistent reconnecting native connection, pending errors, generation filtering, midpoint clock calibration and bounded visual extrapolation. Five original dark responsive routes, fixed accessible global controls/dialogs, root fullscreen, M shortcut, tap tempo, truthful device/loading/errors and denominator/count-in labels. Numeric drafts support ordinary select-all/backspace/sequential typing without 30Hz snapshot interference. No browser audio or remote assets.
- Pinned service-dependencies.json/setup-service.ps1 and web package-lock.json; launcher/start scripts, shared types/CONTRACT, README and real API/browser regressions.

## TDD evidence (focused iterations)

Commands used for native focused cases: project-local build via `pwsh scripts/build-native.ps1`; prepend `.tools/llvm-mingw-20261006-ucrt-x86_64/bin` to PATH, then `build/native/native/native_tests.exe "<case filter>"`. Storage target is `storage_tests.exe`. Frontend command `npm.cmd --prefix web test`; API/browser `node tests/service.mjs` / `node tests/browser.mjs` with runtime PATH.

- Service boundary RED: `node tests/service.mjs` before implementation: `Native service did not start`. GREEN: strict origins, missing/hostile upgrades rejected, malformed JSON/unknown command, persistence and live reconnect checks pass.
- Clock stub RED: Vitest two failures (generation expected false received true; frame expected50400 received0). GREEN clock generation/interpolation tests. Midpoint test RED expected1880 received0; GREEN calibration.
- Storage future-version RED: expected exception, none thrown (1failed/1passed,5assertions). GREEN5assertions/2cases.
- Disabled identity restoration RED: snapshot contains disabledInputs false; GREEN2assertions/1case.
- Actual Windows mode edit RED: audioReady false, Unanticipated host error and frame frozen after unnecessary WASAPI reopen. GREEN actual mode/monitor/external configuration keeps audioReady true and sampleFrame advancing; unchanged externalPortOpens stays stable.
- ApplyNow RED: global Panic stopped running metronome. GREEN scoped live reset, queue-pressure recovery and external release focused cases:12,571assertions/3cases. Live cleared, separate bus preserved, metronome/frame resumes; callback owns sampler mutation.
- Disconnected client RED: error remained null; GREEN visible failure1test.
- Rapid meter browser RED: expected denominator8 received4 from stale compound settings. Native convenience RED unknown command ok=false; GREEN14assertions/1case plus actual API/browser rapid meter/accents and restart persistence.
- Ordinary typing browser RED: typed40 reverted to100. GREEN local draft, actual select-all/backspace/sequential tempo40 and beats6 test.
- Rapid audio channel RED: unknown command ok=false. GREEN21assertions/1case; API parallel disable channels0/1 retained after process restart. First added API assertion incorrectly treated enabled-ID array as booleans; corrected to includes checks and reran GREEN.

## Final verification

- `.tools/cmake-4.4.4-windows-x86_64/bin/ctest.exe --test-dir build/native --output-on-failure` with LLVM PATH: native_protection1.14s, native_piano_render9.30s, native_storage0.05s; 3/3pass,10.53s total.
- `npm.cmd --prefix web run typecheck`: PASS. `npm.cmd --prefix web test`: 2files/4tests PASS,203ms. `npm.cmd --prefix web run build`: PASS,19modules, JS240.69kB/gzip74.90kB, CSS9.82kB/gzip3.03kB.
- `node tests/service.mjs`: PASS130 reconnects reused1 native source ID without held leaks; strict origins/JSON/commands/persistence/WebSocket continuity, rapid meter/accent/channel restart restoration.
- `node tests/browser.mjs`: PASS five tabs, native controls, fullscreen dialogs, settings, reload, mobile width, browser closure, offline resources. Fresh installed Edge; no page errors. Normal numeric typing explicitly covered.
- `pwsh scripts/setup-service.ps1 -Offline`: PASS pinned cached dependencies. `pwsh scripts/start.ps1 -Port18769 -DataDir .tools/launcher-check -NoBrowser`: printed native PID6476; actual GET status contains sampleFrame; stopped owned process afterward.
- Controller CUA inspection at18767: desktop polished/readable; Audio/MIDI and Metronome dialogs accurate; no blocking visual finding. Native Realtek48000Hz/128frames/API22ms; no MIDI input present.
- Scoped diff review and `git diff --check`: PASS (Git line-ending normalization warnings only). No WebAudio/AudioContext implementation; dependencies/downloads/samples remain ignored. No Graphify mutation.

## Limits and concerns

Physical MIDI auto-open/reconnect, hotplug and input-to-audible latency are unmeasured because no keyboard is connected. API output latency22ms is not hardware latency acceptance. Actual device/exclusive changes can gap/reopen and an immediate WASAPI reopen failed on this machine; this failure remains surfaced truthfully, with no claim those hardware transitions are accepted. No30-minute soak performed. Later exercise/import/scoring/tray/release tasks are outside Task3; route placeholders and songs foundations deliberately expose that status.

Upstream compilation emits existing Abseil/CMake optional-dependency warnings. Restricted sandbox lacked loopback access/runtime DLL PATH on an initial final invocation; rerun with authorized loopback and LLVM PATH passed. Vitest sandbox temporary-file ENOENT was avoided by authorized verification context. Disposable preview/test diagnostics were removed after verification; meaningful project tests retained.

## Fix round 1 (review Important findings)

Base839ce164ddd145a97806b12eb772cecc157e2407. Fix commit: c1b5a86c0d742f5fa60b53f04e5335b06cbaa6da.

1. Engine.cpp scheduler overflow now routes ResetLive explicitly to emergencyLiveReset generation recovery rather than emergencySources[0]. Audio consumer clears all live keys/pedals; unrelated future Example events, Playback ownership and global metronome survive. Test `Apply now recovers live ownership with a full non-live future scheduler` in native/tests/engine.cpp fills all8192 scheduled slots with future Example pedal events, applies pending change with held live+pedal, checks live cleared/no deferral, all8192future events delivered, clock and new live ownership resume.
2. Service.cpp catches persistence failures directly around accepted command saves. Both HTTP and WS preserve version1/requestID and expose storage_error with accepted/applied/persisted=false; native result fields preserved. Applied=false for deferred audio edits. Message tells user accepted edit may be lost on restart and to resolve storage before explicit retry. HTTP/PUT status500; invalid JSON parse errors remain separate. PUT settings no longer labels a SQLite failure invalidJSON400. CONTRACT documents semantics.

RED/GREEN commands: `.tools/cmake-4.4.4-windows-x86_64/bin/cmake.exe --build build/native --target native_tests -j4`; LLVM runtime PATH then `build/native/native/native_tests.exe 'Apply now recovers*'`. RED9222assertions: expected1held note, actual2. GREEN45,325assertions/1case. Test refinement moved new live attack until scheduler drains because attacks correctly reject at capacity; counts all8192future hook dispatches. Intermediate compile errors were corrected (dependent json template typing and test insertion), no production bypass.

Actual SQLite failure regression in tests/service.mjs uses Node22's built-in experimental node:sqlite DatabaseSync BEGIN IMMEDIATE on temporary service DB; no production debug seam. `node tests/service.mjs` RED: expected envelope.version1, receivedundefined. GREEN: HTTP42/WSlocked-ws/PUTsettings IDs correlate with storage_error, in-memory edit is visible while locked, persistedfalse, previous0.37 saved volume restores after restart. WS test timeout6s accommodates SQLite's actual3s busy wait; production timeout unchanged. Node logs the expected experimental SQLite warning.

Final after fixes: native rebuild PASS; CTest3/3pass (native_protection1.13s, piano_render9.42s, storage0.05s;10.64s total). Actual API full relevant test PASS including130reconnects/1sourceID, locked SQLite HTTP/WS/PUT and durable restart. Edge browser full relevant test PASS routes/native/fullscreen/normal numeric typing/reload/mobile/offline/browser-close. Frontend implementation/dependencies unchanged, prior typecheck/unit/build evidence retained. Diff check PASS. Scoped self-review complete; no deferred minor polish touched. Temporary browser screenshots removed. Remaining physical MIDI/device-reopen/latency limits unchanged.
