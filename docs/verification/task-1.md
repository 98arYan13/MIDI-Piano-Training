# Task 1 native foundation report

Status: implemented and locally verified; ready for independent review. Commit recorded below after scoped commit. No frontend or later controller tasks implemented.

## Delivered contracts

`native/include/sightreading/Engine.hpp` supplies versioned JSON command/snapshot, asynchronous start/loader, stop, native stereo render, exact sample-frame events and stopped-only input/render hook registration. `native/CONTRACT.md` documents messages, source tokens, scheduling, bus cancellation, snapshots and hook lifetime/realtime restrictions. HTTP integration can serve immediately after cheap construction/start while truthful piano loading progresses. Native WASAPI can run the global clock with a missing piano.

Live/playback/example have distinct sfizz pools; click is the additive native render hook. Source/channel/instance identifies a note; sustain is per source/channel/bus. Virtual note-off/disconnect and pedal-up cannot release physical ownership on the same pitch/channel. Physical pressed state and sonic sustain are distinct snapshot fields. Stopping an exercise bus invalidates its future attacks and releases only its notes, preserving live sound/global hook. Panic is explicit and informs the global hook.

Live physically allocates 1,024 sfizz voices: up to 960 attack allocations, with 64 free slots protected for release allocations. No active or releasing voice is reused. Actual `findFreeVoice` refusal counts include partially admitted layered attacks and release failures, published as `voiceRefusals` and included in overload. Playback/example use separate 256 pools with their own release reserves. Stock region/note/polyphony stealing is bypassed; the independent 614-region mapping contains no choke/single-note caps and disables selfmask. A static assertion rejects regenerated stock 256 capacity. The **tracked** `src/Config.h.in` is patched, since CMake regenerates Config.h; the compile guard caught and prevented the initial generated-file-only mistake.

Bounded producer queue: 4,096 events, 1,024 release reserve; preallocated 8,192 scheduling heap with 1,024 release reserve. Audio consumer uses no application mutex/allocation/IO. Release exhaustion triggers only the pressured source's recovery. Sampler attack preloading is genuine; streaming is through vendor workers. Native mixing uses headroom, finite filtering and soft limiter. MIDI polling/open/reconnect is global every 250ms; no-input WinMM detection prevents repeated RtMidi warning output. External native messages resolve note tokens and aggregate same-pitch/pedal ownership, with explicit release on disconnect/Panic/stop. Hardware MIDI 1.0 instruments cannot provide the internal sampler's per-voice allocation guarantee.

## Provisioning and integrity

Portable Windows compiler was provisioned by parent, not duplicated: LLVM-MinGW20261006 (clang23.1.3), CMake4.4.4, Ninja1.13.2. Official archive URLs/SHA256 checksums are in `native/toolchain.json`. Native preset and build/setup scripts use project-local paths; no IDE or system settings changed.

Pinned official sources/licenses in native/dependencies.json: sfizz1.2.3, PortAudio19.7, RtMidi6.0, JSON3.11.3, Catch2 3.7.1; sfizz recursive submodules are pinned upstream. Compatibility changes: sfizz C++17 for old u8 strings; four atomic_queue dependent-template calls fixed for clang23; optional components disabled and duplicate uninstall target renamed. Vendor assert/debug printing disabled in release. Compiler emitted vendor Abseil deprecated-builtin and old SIMD/CMake warnings; final test/runtime output is clean. No project tests were suppressed.

Official Salamander revision3382bf9496bba2486f5ab0de55a264d1dfc38404: all **664** Sample/Data blobs validated against upstream Git blob IDs, **zero mismatches**; 641 samples total748397030 bytes. assets/piano/manifest.json pins each path/size/blob checksum. Setup downloads pinned raw files and verifies contents. CC BY3.0 license and Alexander Holm attribution (plus kinwie data credit) retained. Mapping generated independently. No reference project code/assets used.

Pinned sfizz protection patch: native/patches/sfizz-protected.patch, SHA256 **27da87ffc6373620d1cfde27d4d631fbb2964965c1dd45b63836e3b15e09dbda**. `git -C .tools/deps/sfizz apply --reverse --check <patch>` succeeded; `scripts/setup-native.ps1 -SkipSamples` verified all dependency revisions/submodules and already-applied patch idempotently. Downloads/tools/deps/builds/samples are ignored and excluded from the scoped commit.

## TDD evidence

Ignored build/native logs retain observed failures and fixes, not inferred failures:

- Queue standalone RED rejected its first valid attack. GREEN admitted attacks and kept eight reserved release slots; concurrent three-producer regression preserves all3,000 releases.
- `red.log`: stock allocator stole held C4 (voice60 became96), reused a releasing tail (60 became70), and failed release reserve (four attacks accepted instead of three). `green.log` verified free-only allocation and reserve.
- `owner-red.log`: same-pitch owner202 note-off released owner101. `owner-green.log` verifies targeted owner release.
- `engine-red.log`: five genuine skeleton failures (not-ready, submit, ownership, pedal, deferral). `engine-green.log` verifies real implementation.
- `scheduler-red.log`: stopped playback's queued future note returned, leaving two held notes instead of one. `scheduler-green.log` verifies bus-generation cancellation and continuing render hook.
- `lifecycle-red.log`: Panic did not notify global hook and missing piano prevented available audio clock. `lifecycle-green.log` verifies both fixes.
- `capacity-red.txt`: requested1,024 actually admitted192 because stock compile cap256/reserve64. Config template fix verifies960 active attacks. `capacity-counter-red.txt`: refusal getter was zero instead of240. `capacity-counter-green.txt`: **17,166 assertions / one case passed**, actual960 attacks +240 refusals, first C4 held intact, targeted release creates the961st voice. Final layered regression confirms an attack can start one layer while another allocation refuses, and the refusal still increments.

Offline tests enable sfizz freewheeling to await streaming deterministically; realtime operation disables it. Initial streaming-starved diagnostic runs were corrected rather than treated as allocator evidence. External MIDI ownership cleanup was implemented from source inspection; it lacks a connected-hardware red/green test and is explicitly part of hardware coverage limitations.

## Final commands and results

`scripts/build-native.ps1 -Tests` performs release configure + native Windows build + full CTest. Final `build/native/final-suite.log`: **2/2 passed**, native_protection1.02s, native_piano_render8.84s, total9.92s.

With portable compiler bin on PATH, `build/native/native/native_tests.exe`: **19 test cases / 47,078 assertions passed**, no failures (`final-cases.txt`). Focused tests were run during each change; final suite rerun after last native ownership edits. Tests use real FLAC sample decoding, real sfizz voices and production queue/scheduler rather than mocked sampler calls.

`build/native/native/native_probe.exe build/native/piano-proof.wav`: genuine sampled PCM16 stereo48k WAV, **1,447,936 frames /30.1653s**. All88keys (A0–C8) audible at velocities20,64,127; pedal retention/release verified. Peak0.062360499, RMS0.0072770966, minimum key render energy1.4959052e-5; every rendered float finite, zero overload. `final-wav.json` records standalone preload1784ms. Mapping supplies16 velocity layers; the audio probe exercises three representative velocities, not every layer. WAV is retained as ignored review evidence. Independent PCM16 readback verified5791788 bytes, peak integer2043 (no clipping), and2316909 nonzero samples.

`build/native/native/native_probe.exe --live`: actual native WASAPI **audioReady=true, pianoReady=true**, Speakers(Realtek USB Audio), actual48,000Hz/128frames, reported output latency22.0ms, process ready2067ms, zero reported underflows during the approximately2.5s note/release probe (`final-wasapi.json`). Initial real WASAPI error Invalidnumberchannels was diagnosed to shared-mode format conversion and corrected using PortAudio's shared auto-convert flag; native stream then opened successfully. This is a process-start measurement with provisioned/cached files, not a cold boot benchmark or MIDI-to-audio measurement.

## Remaining acceptance limits

No physical MIDI input enumerated. Microsoft GS Wavetable Synth was enumerated as an output, but physical external-instrument sound/ownership/latency were not tested. Hardware connect/reconnect/CC64 behavior, exclusive WASAPI device compatibility, wired MIDI-to-audio p95 and30-minute dropout soak remain unmeasured. API-reported22ms output latency does not prove the requested MIDI latency target. HTTP/UI startup<=2s belongs to later service integration; this task provides nonblocking loading hooks. Strict no-steal allocation is verified for the internal sampler and does not control an external instrument.

Commit: `d35fb9243ecba3485d58e60a2e96c58ac93ec71d` — feat(native): add protected sampled piano and global MIDI foundation. Scoped27-file commit; final Git status clean. Git whitespace check passes excluding the unified patch, whose required context-only blank lines trigger whitespace warnings; patch reverse-check and setup verification pass.

## Review fix round 1 (base d35fb92)

Both Important findings reproduced before routing/namespace changes. Scope: native/include/sightreading/Engine.hpp, native/src/EngineInternal.hpp, native/src/Engine.cpp, native/src/Commands.cpp, native/tests/engine.cpp, native/CONTRACT.md. No deferred ledger items or later-task behavior changed.

Added a stopped-only native external message sink that overrides RtMidi delivery for deterministic embedding/tests. It captures production routing bytes on producer threads under externalControl, uses fixed capture storage, and does not replace actual sampled piano. Its context lifetime and no-reentry requirements are documented.

RED command: portable clang DLL directory on PATH; CMake --build build/native --target native_tests -j4; build/native/native/native_tests.exe '[routing]'. review1-red.log: 3 cases failed /273 assertions, specifically external monitoring disabled still emitted2 messages instead of0; disabling after owned attack/pedal still emitted4 instead of2; reserved midi:fixture keyboard#0 virtual disconnect returned ok=true instead of invalid_params. New sink was wired before this run, with original routing decisions unchanged.

Fix: external On sends and ownership allocation require monitoring=true; pedal-down state/sends require monitoring=true. Existing note-off, pedal-up, Disconnect and Panic release branches remain unconditional under external mode. Muted attacks do not leave external ownership entries. All virtual command types reject the exact native-owned midi: namespace before source lookup/allocation, preventing deliberate virtual/physical identity collision.

GREEN same focused command: review1-green.log, 3 cases /548 assertions passed. Tests verify blocked attack/down, allowed pedal-up, previously sounding token note-off after monitoring disabled, and rejection of all four virtual event types using reserved physical identifier while an injected physical sustained note remains sounding/owned.

Full release configure/build/CTest: scripts/build-native.ps1 -Tests; review1-full.log, native_protection1.07s, native_piano_render8.90s, 2/2 passed, total10.02s. Direct native_tests: review1-cases.log, 22 cases /47,626 assertions passed. Native piano render remains genuine FLAC/sfizz. Hardware limitations unchanged: injected message bytes prove production routing decisions, not external instrument response or physical MIDI timing. Deferred external-output recreation and disconnect/bus-stop InputHook ledger items remain for integration review.

Review fix commit: `e9f65e325ec87f94b49df8dbe511fade8ecc0b90`. Scoped6-file commit; git diff --check passed and final Git status clean.