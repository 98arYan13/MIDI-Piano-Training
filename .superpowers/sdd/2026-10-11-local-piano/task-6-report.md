# Task 6 implementation report

Status: DONE_WITH_CONCERNS (physical hardware coverage only). Base c22c044ae8b3b6427e9be75e543dd48a24d7188a; final commit SHA is provided in the controller handoff because this report is included in that commit.

Implemented native binary MIDI import, durable library/original download, recoverable trash, five independently authored starter arrangements, Play Along transport/scoring/track assignments, controller-aware seek/loop restoration, native accompaniment and the functional browser library/transport. Components are separated into Song, Library, LibraryRoutes and Transport, with dedicated web Library/PlayAlong. Service composes consumers behind the existing bounded callback capture ring; native callbacks perform no JSON/storage/scoring/locking. Live admission and protected voice allocation remain unchanged. Source allocation now explicitly excludes reserved application sources125–127.

## TDD evidence

Commands ran from repository root after `./scripts/build-native.ps1` (each build recorded in its matching `build/task-6-*-build.log`), with project LLVM bin on PATH. Focused command form: `build/native/native/native_tests.exe '[tag]'`; storage form `build/native/native/storage_tests.exe '[library]'`.

| Area/tag | RED log / observed failure | GREEN log / result |
|---|---|---|
| Binary SMF `[song]` | task-6-song-red.log: unimplemented import throws | task-6-song-green.log:14 assertions/2 cases |
| Library `[library]` | task-6-library-red.log: unimplemented store throws | task-6-library-green.log:7 assertions |
| Transport `[transport]` | task-6-transport-red.log: placeholder transport/inference fails | task-6-transport-green.log:10 assertions/2 cases |
| Existing voice CC `[controllers]` | task-6-cc-red.log: mute leaves0.2886 energy | task-6-cc-green.log:6 assertions |
| Empty/loop snapshot `[transport]` | task-6-loop-red.log: null JSON access | task-6-loop-green.log:19 assertions/4 cases |
| Miss/retry `[scoring]` | task-6-score-red.log: wrong+deadline counts2 instead of1 | task-6-score-green.log:6 assertions |
| Zero-duration event order `[song]` | task-6-order-red.log: missing canonical order | task-6-order-green.log:37 assertions/5 cases |
| Reserved identities `[sources]` | task-6-source-red.log:125th named source accepted | task-6-source-green.log:125 assertions |
| Channel CC120 release tails `[controllers]` | task-6-cc120-red.log: left tail energy0.753 survives | task-6-cc120-green.log:10 assertions/2 cases |

Expanded binary fixtures cover SMF1 tempo/key/meter/channel ordering, note overlap/velocity-zero Off, malformed metadata/status/VLQ/EOT, event/track/time/tick bounds. Expanded transport fixtures cover inclusive150ms chord assembly independent of tolerance, speed/tempo, unsupported Follow, pathological accompaniment density, sustain restoration, live ownership and controller stereo isolation. Intermediate edge suite: build/task-6-edge.log,70 assertions/12 cases. All meaningful tests retained.

## Final verification

- `./scripts/build-native.ps1` — PASS, build/task-6-final-build.log. Initial final build exposed Windows psapi prerequisite include order after formatting; corrected windows.h-before-psapi.h and rebuilt.
- `.tools/cmake-4.4.4-windows-x86_64/bin/ctest.exe --test-dir build/native --output-on-failure` with portable LLVM DLL PATH —4/4 PASS in11.13s: protection, piano render, storage, transport render; build/task-6-final-native.log. Native file transaction tests use normal Windows permissions because restricted token denied rename.
- web `npm.cmd run typecheck`, `npm.cmd test`, `npm.cmd run build` —PASS,12 tests/5 files; build/task-6-final-{typecheck,web,web-build}.log. Existing >500kB bundle advisory remains.
- `$env:PATH = "$PWD/.tools/llvm-mingw-20261006-ucrt-x86_64/bin;$env:PATH"; node tests/play-along.mjs` —PASS actual multipart binary import/original-byte equality/window/title/assignment/trash/restore/restart, native playback/loop/accompaniment/live/metronome, installed Edge UI/upload/mobile; no browser errors. build/task-6-final-integration.log. First final launch omitted portable runtime PATH and could not start; corrected environment, complete suite passed.
- Same environment: `node tests/service.mjs`, `node tests/browser.mjs`, `node tests/keyboard-notation.mjs`, `node tests/staff-practice.mjs` —all PASS, matching build/task-6-final-{service,browser,keyboard,staff}.log. Existing130 reconnects reuse1source; storage error correlation and native Staff scoring remain intact.
- Fresh pristine sfizz pinned HEAD archive: `git apply --unidiff-zero --check`, apply, reverse-check —PASS; patch SHA256 in native/patches/sfizz-protected.sha256; setup verifies digest before guarded application. Owned extracted proof and tar deleted after resolved-path check. Zero-context patch is guarded by exact pinned revision and digest.
- Parent CUA review at18774 confirmed starter/library/Play, continuous Grand notation/duration bars/moving NOW/playback keys and native counters/global toolbar. No blocking visual issue. Parent accepted Task8 polish for title/BPM drafts/compact layout.

## Audio evidence and tradeoff

`build/native/native/transport_probe.exe build/native/transport-proof.wav` (CTest also runs probe) uses actual Salamander render. build/task-6-audio-proof.log: expected/observed note frames256,32256,64256 exactly; Independent100bpm clicks0,28800,57600. Playback stop at96000 preserves live voice999 and clicks. Four-second16bit WAV finite, peak0.1375106722,RMS0.00394393935. Measured preload1749ms, render241ms for4seconds, process working/peak set324239360bytes. These are offline process measurements, not hardware latency/dropout claims or causal before/after benchmarks. Channel extension adds16KiB preallocated stereo curves and two gain multiplications per playback voice/sample; no extra samples/synths. Buffer lifetime precedes synth destruction, reuse clears pointers, rendering chunks cap128 and curves index absolute callback block frames. Real stereo tests cover held voice delayed64-frame gain/pan changes, isolated channel/live and release layers/CC120.

## Frozen Task7/8 interfaces and explicit limits

See native/CONTRACT.md for exact endpoints, transport commands/snapshot, native scoring and source constants. Task7 must use ExampleSource125/Bus::Example and compose behind existing hooks. SongSource126 and AccompanimentSource127 are reserved; named/physical sources1–124 cannot collide. Browser visualization uses native frame clocks and canonical event spelling/durations, no WebAudio.

Strict imports20MB/1million events/256tracks/2hours additionally reject cumulative ticks above INT32_MAX (official parser representability). Original raw controllers/bend are retained; audible piano mappings CC64/7/11/10/120/123; other CC/bend have no piano audition mapping. SQLite BLOB is canonical original; original-file staging/compensation handles ordinary transaction errors. A crash between file publication and DB commit can leave an unreferenced cached file; no partial library record is visible. Recoverable trash retains canonical originals.

No hidden bar-count import cap. Lazy accompaniment window inference caps16384 overlaps and128 scheduling steps per pump, skips empty spans, publishes visible accompanimentError and stops accompaniment only on overload. Original playback/live/metronome continue. Unsupported Follow maps publish followError rather than falsely reporting clamped sync. Playback scheduling is bounded2048 events/pump and native queue overload is actionable. Window API caps4096 notes.

Five starter files/provenance/license are committed in assets/music; deterministic generation script retained. No reference project code/assets copied; official pinned midifile BSD2 dependency and attributed Salamander remain local ignored dependencies. No Graphify/global install/remote changes.

Owned test processes/data are cleaned by suites. Preview copied executable PID26372 was stopped after verifying its owned build path; preview data/logs/executable removed after resolved path checks, parent closed browser tab. WAV/test logs remain ignored review evidence. No physical MIDI keyboard, external synth, speaker loopback, exclusive-device behavior or long hardware soak was measured. These hardware checks remain Task8 coverage work, not missing functional requirements.


Final setup command: ./scripts/setup-native.ps1 -SkipSamples PASS, build/task-6-final-setup.log. Restricted token initially prevented Git MSYS named-object creation; normal Windows permissions completed cached dependency/submodule and patch-integrity verification. .gitattributes pins patch/digest LF to keep hash reproducible on checkout.
