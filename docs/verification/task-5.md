# Task 5 report

Status: DONE_WITH_CONCERNS (implementation/automated verification complete; hardware acceptance and compact Grand layout remain).
Base: 3f6d1e302a5479a5625a21be2a8f1d0e05b951da.
Commit: c22c044ae8b3b6427e9be75e543dd48a24d7188a.
Branch: codex/local-piano.

## Scope and files

- native/include/sightreading/Practice.hpp, native/src/Practice.cpp, native/tests/practice.cpp, native/CMakeLists.txt: reusable native pitch spelling, all Staff generators/config validation, deterministic bounded search, physical-owner tracking and exact sample-frame Wait/Scroll scoring.
- native/src/Service.cpp: control-worker integration behind existing bounded InputHook ring; persisted canonical config, atomic partial edits/list toggles, correlated commands, native practice snapshots/change events, ring-gap recovery. No audio callback theory/scoring/JSON/lock/IO work added.
- native/src/Engine.cpp: actual Disconnect/StopBus and emergency source recovery now notify InputHook; existing Panic/ResetLive behavior retained. No live allocation or metronome ownership changes.
- web/src/StaffPractice.tsx, types.ts, App.tsx, practice.css: actual Staff route with every specified control, native targets/results/counters/time, local numeric drafts, settings/fullscreen and stable revision-memoized TimelineNote arrays. Global keyboard/provider remain mounted. Retired unused StaffPreview.tsx fixture.
- web/src/notationNote.ts, notationNote.test.ts, ScrollingStaff.tsx: production VexFlow note creation consumes native spelling, meaningful Cb/B# glyph position/accidental test; SVG glyph overflow and removal of clef shadow fix current-target clipping/perceived line gap.
- tests/staff-practice.mjs: retained real native API + fresh Edge coverage. tests/keyboard-notation.mjs: retained original keyboard coverage, replacing fixture scroll/freeze with real Scroll/Start/Stop and waiting for native Grand snapshot.
- README.md, native/CONTRACT.md: operation, semantics, native/controller/hook/protocol interfaces and coverage limits.

No reference code/assets, new downloads/dependencies, system-wide installation/settings, Graphify, subagents or model escalation were used. Tools/samples/caches/build/test screenshots/logs remain ignored.

## Implemented behavior

Default treble/random/C-major/1-note/2-hands/smoothness3/Wait. Staff supports Treble/Bass/joined Grand/chord symbols; global 88-key keyboard remains available. Random1–5 notes, one/two-hand octave span constraints and chord-based targets; diatonic triads/inversions, seventh open voicings; Autumn Leaves/50s/circle/basic tritone substitution; fixed five-finger single-note position drills; selected diatonic2nd–7th intervals. All12 named keys + Chromatic, major/natural/harmonic/ascending melodic minor. Chord symbols support ordered/random diatonic degrees, multiple selected keys,3/4 tones, common pitch classes any/1/2 and an inclusive MIDI ignoreAbove threshold shown with note name. Native key spelling includes double accidentals where musically required.

Bounded seeded search4096 candidates/target; validation generates the complete64-target session before accepting/persisting a partial patch. Empty/incompatible settings give actionable errors atomically. Smoothness0 unrestricted;1–6 bounds target-center movement12→2 semitones. For Position it bounds single-note movement within one fixed five-diatonic-note hand. StartingFinger1–5 changes the first pitch; later target fingering matches its position. Chromatic progressions pick one seeded tonic for the complete degree sequence; other Chromatic tonal targets choose seeded tonal roots rather than chromatic clusters.

Wait checks the exact physically held pitch set, with all target pitches freshly attacked within inclusive150ms. Wrong attacks count misses; wrong held pitches block completion until released/retried. Sustained sound never answers. Duplicate same-pitch owners remain separate for cancellation but represent one pitch for correctness. Pre-start/reset-held keys require fresh attacks; reset retains audio/physical state. Scroll uses exact rational native quarter boundaries and inclusive adjustable ±tolerance (default150ms); incomplete chords count one missed target at deadline. Stop/settings/navigation/reset never stop live notes or global metronome. Native Follow resync uses the same captured origin on Start/Reset; Stop retains global clicks.

## RED → GREEN evidence

Initial meaningful RED command, after adding interface/empty behavior stubs and fixing missing Catch string matcher include:

```powershell
./scripts/build-native.ps1
$env:PATH="$PWD/.tools/llvm-mingw-20261006-ucrt-x86_64/bin;$env:PATH"
.tools/cmake-4.4.4-windows-x86_64/bin/cmake.exe --build build/native --parallel 4
build/native/native/native_tests.exe '[practice]'
```

Initial actual result:5/5 cases failed; empty theory0 notes vs>=49, generator0 targets vs24, Wait misses0 vs1, Scroll deadline misses0 vs1, actual Engine disconnect hook still reported On instead of Disconnect. First GREEN:5 cases/1929 assertions. A nlohmann brace initializer initially wrapped config in an array; changing it to object assignment fixed the intermediate Unknown setting range/mode failure.

Expanded focused RED/GREEN runs used the same build + `[practice]` command:

- Wrong-held exact-set completion incorrectly hit; incomplete Wait chord did not expire after7200 frames; chord-based5-note pool had insufficient unique candidates. Fixed exact pitch set, native window expiry and chord/octave pool. Open-voicing regression then showed span9 after inversion; sorting before lifting the second voice fixed it.
- Basic substitution root7 vs expected flat-II1, Chromatic triad third2 vs actual major/minor third3/4. Fixed tritone substitution and tonal-root Chromatic chord generation.
-137BPM target63 frame1324323 vs rational1324379 showed accumulated integer-quarter truncation. Fixed all target/clock/deadline boundaries to index*rate*60/tempo before rounding.
- Ring-gap recovery stub left running=true; fixed stop/rebuild-only-live-held/error behavior.
- Position originally returned5 simultaneous pitches instead of one finger-selected pitch. Controller Ruling7 clarified fixed five-finger single-note drill; first-target/finger/bounds tests failed, then passed after implementation.
- Incomplete Scroll chord counted2 misses at the same deadline instead of1; fixed timeout clearing vs native deadline miss.
- Chromatic 50s progression changed tonic between steps (root11 vs expected10); fixed one seeded session tonic.

Final focused result:16 native cases /2508 assertions passed. Count changed after replacing5-note Position chords with single-note targets, reducing repeated range assertions while adding finger semantics checks.

Renderer regression proof: `npm.cmd test -- notationNote.test.ts` with production spelling letter deliberately replaced by B failed Cb glyph y95 vs expected125; restoring production spelling passed. This is an actual VexFlow StaveNote position/modifier assertion, not an object-literal-only test. Actual API/Edge then consumed native Gb-major MIDI59→Cb4, checked flat glyph U+E260 at y125, changed to C-major B3 and checked y130.

`node tests/staff-practice.mjs` initially proved practice.answer missing (invalid_practice, expected ok:true). Added action:on/off forwarding to ordinary native virtual commands; actual sound/input-hook path and scoring passed. It never directly mutates score.

Browser harness iterations exposed native30Hz vs immediate fixture assertions (Grand, Stop/frozen transform, queued metronome start). Assertions now wait for native/DOM state. Controlled native checkboxes use click then native-state checks instead of assuming synchronous checked state. First unprivileged loopback run timed out; the authorized local socket/Edge invocation passed. No product fallback was added.

## Final verification and output evidence

All paths below are relative to project root; logs are ignored build artifacts.

- `build/native/native/native_tests.exe '[practice]'`:16 cases/2508 assertions PASS. Output: build/task-5-practice-final.log.
- `.tools/cmake-4.4.4-windows-x86_64/bin/ctest.exe --test-dir build/native --output-on-failure` with portable DLL PATH: native_protection, native_piano_render, native_storage all3/3 PASS,10.08sec. Output: build/task-5-native-final.log.
- In web: `npm.cmd run typecheck`, `npm.cmd test`, `npm.cmd run build`: exit0;5 files/12 tests PASS; Vite120 modules, bundle977.91KB/468.54KB gzip. Output: build/task-5-web-final.log. Known non-fatal>500KB bundled VexFlow/font warning remains.
- One logged web invocation at23:06:48 failed nativeClient.test.ts before test collection with ENOENT for a sandbox SSR temporary file; other4files/11tests passed. Fresh identical complete rerun at23:07:17 passed5files/12tests, typecheck and build without source changes. The log preserves both failure and clean retry; the transient failure is not counted as product success.
- `node tests/service.mjs`: PASS strict origins/JSON/commands/persistence, actual WS continuity,130 reconnects reuse one source without leaked notes, locked SQLite correlated HTTP/WS/PUT errors and durable state. Node SQLite experimental warning is non-fatal.
- `node tests/browser.mjs`: PASS five routes/native/global controls/fullscreen/settings/reload/mobile/browser-close/offline resources.
- `node tests/keyboard-notation.mjs`: PASS Grand/88/C4, real same-pitch source ownership, genuine multitouch cancellation, computer chords, route/live isolation, virtual sustain/focus recovery, M, responsive keyboard pan/fullscreen/native clock.
- `node tests/staff-practice.mjs`: PASS actual Wait/Scroll, exact held pitches, real practice.answer on/off, reset/stop with unrelated held browser owner + global clicks, invalid config atomicity, all generator families/visible options/minor scales, Grand/88, local numeric drafts, native enharmonic SVG geometry, fullscreen/restart persistence/no browser errors.
- Full four-script output: build/task-5-integration-final.log. The shell fails on any nonzero script exit; every final script exited0.
- `git diff --check`: exit0 after scoped EOF whitespace cleanup.

## Frozen interfaces for Tasks6/7

Canonical schemas and detailed semantics are frozen in native/CONTRACT.md and web/src/types.ts. Native model: SpelledPitch, PracticeTarget; scalePitches/defaultPracticeConfig/validatePracticeConfig/generatePractice. Practice methods configure/start/stop/reset/input/tick/snapshot/revision/recoverHeld. Control-worker-only, no internal mutex: caller must serialize. Keep supplied sample rate/origin consistent. Task6 song transport may compose its own timeline/scorer behind the same event pump using these spelling/physical-owner/window rules; Staff's64-quarter session itself is not a song tempo-map model.

Service owns practiceMutex. Existing Engine InputHook/BeatHook remain installed once, before start; contexts are the preallocated Hooks object declared before Engine and remain alive through destructor Panic. InputHook only copies into the bounded ring. Consumers get exact applied InputEvent frames. Later controllers must compose behind current hooks/pump instead of replacing registrations. Pump captures frame before draining events, then ticks controllers; this avoids advancing past an event still awaiting drainage. No RenderHook owner was replaced.

Source/channel/instance identify physical owners. Disconnect clears only that source; non-live StopBus leaves live pressed state; Panic/ResetLive clear live pressed/attempt state. Synthetic cancellations never answer or count misses. Reset clears attempts/counters/origin but preserves pressed map and audio. Fresh On required after reset; pedal and playback/example ignored for answers. Ring loss stops evaluation and reconciles only live held notes from engine state with visible actionable error; Start clears error. Actual Engine Disconnect/StopBus regression plus real API ownership/reset tests cover the boundary.

Native command envelope remains version1/request-ID correlated. practice.configure merges partial fields atomically; practice.option toggles intervals/keys atomically; persisted config excludes transient exercise state. practice.answer is a validated real-note on/off adapter, with WS source remapping retained. practice.start/reset/stop do not own live/click audio. WS practice change events contain exact sampleFrame/revision/index/hits/misses/running; full snapshot is authoritative. NativeProvider/PianoKeyboard remain global. Native notes keep a React reference while revision unchanged, retaining SVG memoization. Renderer is visual-only; no JS generation/scoring/audio schedule.

## Visual review, lifecycle and limits

Controller CUA reviewed http://127.0.0.1:18772, PID24880, Grand settings/keyboard. It found the current x0 target glyph clipped and a perceived~72px staff gap; own before/after screenshots confirmed SVG clipping and clef shadow. CSS overflow:visible restores current notehead/accidental; removing shadow restores continuous lines. Controller reload confirmed both fixes. Ignored screenshot evidence: web/test-results/staff-before.png, staff-after.png, staff-practice.png, keyboard-grand.png/keyboard-mobile.png.

Controller closed its tab; verified owned build/staff-preview.exe PID24880 was stopped. Disposable build/staff-visual.mjs, build/staff-preview.exe and build/staff-preview-data were removed after resolved absolute paths were checked under the project build root. All meaningful native/web/API/browser tests remain. Harnesses close their owned native/Edge processes and delete only their own temporary databases.

No physical MIDI device was available. Service tests use actual C++/WASAPI/Salamander plus independent virtual owners at the boundary, not a hardware timing claim. Actual hardware hotplug, wired MIDI/virtual-to-audible p95, output-device/exclusive rebuilds,30-minute dropout soak and dense10-minute song performance remain release acceptance checks. Grand+keyboard needs page scrolling at1265×713; controller accepted that and deferred compact training layout to Task8. No Task6 import/playback or Task7 Ear/Flash work was implemented.

## Scoped self-review and commit

Reviewed generator modes/key spelling/ranges/spans/smoothness/common classes, physical owner cancellation, rational native timing, service mutex/hook lifetime/source mapping/persistence, UI controls/clock memoization and scoped diff. Findings were fixed with the focused regressions above. Parent/controller owns the subsequent independent review gate.

Scoped commit: Add native Staff generation and exact-frame practice scoring. Final git status --short produced no entries; git diff --check exited 0. Commit SHA: c22c044ae8b3b6427e9be75e543dd48a24d7188a.


