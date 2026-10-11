# Local piano release verification — 2026-10-12

## Final whole-branch fix wave

All eight final findings have product fixes and focused regressions: scope-aware Panic/StopBus recovery at full event/scheduler capacity, complete unassigned Flash triads in single clefs, authoritative paused/stopped notation redraw, truthful dense-window message, portable contract link, bounded tray shutdown/failure diagnostics, named-source capacity diagnostic, and wide Ear integer validation. Scope controls ignore unrelated default-source generation recovery; ResetLive and bus generations remain separate. Independent scoped rereview APPROVED `7b040690ec9177c519616d18fea23807d47d7e61..03e6867c088666cd563735463e45efacf433c5ce`: all3Important+5Minor resolved, no newCritical/Important and0unresolved findings. Product is frozen. Existing fourteen rulings and original Task1–8 evidence below are preserved.

Frozen final native executable SHA256: `8A4368C59DADB2647ED458ED017218BA917AA8AF4484938FE22223895A84B0C8`. Copied soak and canonical portable `build/release/SightReading` binaries match. The new exact-binary real WASAPI run completed `2026-10-11T22:35:58.428Z–23:05:58.624Z`:1,800,196ms/7,144iterations,48kHz/128frames/API22ms, zero underflow/overload/voice-refusal deltas, retained heldLive and metronome throughout and at end. Native looping playback/HearPractice/Triad, Examples and137BPM sixteenth clicks ran with repeated Live attacks plus heldC4. [Exact-binary soak](verification/final/soak.json), [fix/review proof](verification/final/fix-proof.json). The earlier Task8 soak below remains historical and does not certify this changed callback binary. No audible capture or physical MIDI was used.

| Final check | Evidence |
|---|---|
| Native focused RED/GREEN | Initial full-heap Panic/scoped stop/newer attack and wide integer tests failed; default-source recovery then reproduced skipped scope controls. GREEN79,573 assertions in8focused cases, including queue reserves/registry/atomic config and injected tray registration/icon/wait failures. |
| Full native CTest |4/4 passed13.70s; [log](verification/final/native.txt). Sandbox SQLite rename denied; unchanged suite passed in normal Windows context. |
| Frontend |15Vitest tests, typecheck and production build passed. Existing VexFlow/font chunk warning remains visible, with unchanged threshold. |
| All native/Edge integration |Seven suites passed, including the new actual Flash glyph geometry/cache and paused seek/stopped second-song regression; [log](verification/final/e2e.txt), [notation proof](verification/final/notation-proof.json). |
| Copied offline release |System32-only runtime PATH, four linked portable documents, five tabs, retained imported library,208ms cold UI/1596ms piano+WASAPI ready; [proof](verification/final/release-proof.json). |
| WindowsPowerShell5 launcher |System32-only PATH, native tray window, actual audio readiness and graceful shutdown passed; [log](verification/final/launcher.txt). |

The new headless run measured blank55.583fps versus cached dense56.275fps, moving54.948fps/max124.9ms; cached max18.6ms. Late ten-minute song window first78.59ms/cached median1.616ms/max2.841ms. Compact Grand and Play keyboard bottoms711.94/711.25px at1265×713; full88keys remain available through horizontal scrolling. Ruling14 and absolute60fps unverified status are unchanged. [Grand](verification/final/compact-grand.png), [Play](verification/final/compact-play.png), [stopped second song](verification/final/notation.png).

One-minute mixed-load process sampling on this exact binary measured60,654ms,19.578% of one logicalcore/1.224% of16logicalmachine, working set~483.7MB; [process proof](verification/final/process-load.json). This is whole-process CPU, not callback-only or maximum-voice pressure. Runtime PE imports checked for exact exe and two bundled DLLs; [imports](verification/final/pe-imports.txt).

Physical MIDI/hotplug/wheel/external-instrument and wired key-arrival-to-audible p95 remain unmeasured. No acoustic capture or audible-dropout certification is claimed. Native callback counters/sample positions/ownership and decoded sample renders are the measured coverage.

## Historical Task8 verification

Release scope: complete approved five-tab native Windows app, Task8 fixes and offline portable packaging. Base b6e7f9e53fc403faa26bed75d7187b644ca3ac01. Exact final Task8 commit is recorded in the ignored worker report and packaged release-manifest.json after commit. Native audio engine/callback/sampler patch unchanged in Task8; native modifications are integer validation, worker-only window indexing/cache, service lifecycle and tray.

## Machine and methods

Windows11 x64; Intel Core i5-14400F,10cores/16logical; RAM34193854464bytes (~31.85GiB installed32GB), C: free~36.76GB at preflight. Portable LLVM-MinGW20261006/Clang23, CMake4.4.4/Ninja1.13.2; Node22.16.0/npm11.4.1 build/test only. Installed Edge155/Chrome154; tests use fresh headless Edge contexts, no browser downloads or unrelated browser profiles. Realtek USB Audio shared WASAPI actual48000Hz/128frames, reported API output latency22ms. API output latency is not actual key-to-audible latency.

## Fresh final checks

| Command | Result |
|---|---|
| `pwsh scripts/build-native.ps1 -Tests` | Build succeeded; CTest4/4 passed12.20s: native protection, sampled piano render, SQLite/library, transport render. |
| `npm.cmd --prefix web ci --cache .tools/npm-cache` |93 packages installed/94audited,0vulnerabilities. |
| `npm.cmd --prefix web run typecheck` | Passed. |
| `npm.cmd --prefix web test` |14tests/5files passed. |
| `npm.cmd --prefix web run test:e2e` | All six maintained suites passed: service, browser, keyboard-notation, staff-practice, play-along, ear-flash. |
| `npm.cmd --prefix web run build` | Passed;1,007.14KB JS/477.61KB gzip. Unmodified500KB advisory remains; measured cold UI230ms. |
| `node tests/release.mjs` | Copied runtime/System32-only PATH/all routes/no external requests; real19200-note10minute import; title/assignment invalidation; browser flow; retained library on graceful restart. |
| `pwsh tests/launcher.ps1` | Built-in WindowsPowerShell5 start/stop, clean PATH, actual native tray window, WASAPI/piano ready, graceful native exit. |

Focused RED→GREEN:
- Native seed4294967296 previously wrapped to0: `cmake --build build/native --target native_tests --parallel4` then `native_tests.exe '[bounds]'` RED failed REQUIRE_THROWS. Wide signed/unsigned validation GREEN7assertions/1case incl UINT64_MAX/negative/bool/nonintegral/0/INT_MAX.
- Window index test first failed build with missing SongWindow; after native interval tree GREEN4assertions/1case for10000notes, late spanning note, limit and<100 inspectednodes. API release fixture exercises actual19200notes/cachedwindows/assignment mutation/restart.
- `npm --prefix web test -- notationModel.test.ts` RED missing spelledLabel; GREEN nativeCb/doubleaccidental labels. Next RED missing segmentFingerprint; GREEN same-segment spelling/result changes invalidate while unrelatedwindow notes keep cachedSVG. Final suite includes both.
- `node tests/shutdown.mjs` RED authenticated shutdown404; GREEN unauthenticated403/authenticated200 then service ends gracefully.
- `pwsh tests/launcher.ps1` RED10second readiness timeout under WindowsPowerShell5: Invoke-WebRequest's legacy HTML parser was unavailable. `-UseBasicParsing` GREEN actual portable startup/stop, avoiding any global/system setting change.
- Common1265x713 release layout REDGrandkeyboardbottom842.94px, then734.94px; compact rows/native Grand geometry GREEN711.94px. Play Along initially belowfold; compact title/transport/position/settings/library rows GREEN711.25px. Global toolbar/settings remain accessible; controller CUA approved both clefs/brace/continuouslines/NOW/full88keys and all12Guide topics.

## Performance evidence

Maintained `tests/release.mjs`: copied actual executable with environment PATH only C:\WINDOWS\System32; native actual audio/piano readiness1247ms. Cold fresh-context navigation to native-connected Staff notation230ms. Generated binary SMF0 PPQ480 with19200notes over~600seconds. Late window470000–474000ticks: first parse/index69.71ms;100 cached queries median1.477ms/max3.073ms. Response indices<1000nodes, spanning-note native test<100. Current-song-only HTTP cache invalidates on committed import/title/recent/assignment/trash/restore/restart; Transport owns its loaded index. At most4096 public window notes with explicit truncated diagnostic, originals/audio retained.

Five-second rAF sampling on same installed headless Edge/host: blank-page baseline55.858fps/median17.7ms/p9518.2ms/max71.6ms; dense movingnotation55.162fps/median18ms/p9518.1ms/max106.2ms; already-cached dense notation56.097fps/median18ms/p9518.1ms/max18.6ms. Cached run uses slow native speed0.25 and spacing40, after750ms stabilization, within one segment; moving run uses ordinary native playback and window/segment changes. Focused memoization retains existing SVG when that segment's notes are unchanged; no glyph/pitch/spelling/duration simplification. Dense movement shows transient SVG creation costs, no sustained≥250ms stalls in this measured run. Ruling14 keeps absolute60fps as target/unverified, since blank host baseline itself~56fps; no literal60pass or warningthreshold inflation. Further real60Hz-display profiling remains a hardware follow-up.

Exact structured evidence: [release-proof.json](verification/release-proof.json), [process-load.json](verification/process-load.json), [compact Grand](verification/compact-grand.png), [compact Play Along](verification/compact-play.png). Controller CUA independently inspected copied preview18783 at1265x713.

## Audio soak and limitations

Real wall-clock run 2026-10-11T21:40:25.436Z–22:10:25.626Z: 1,800,190ms,7158 iterations. Actual shared WASAPI48000Hz/128frames/API22ms. Mixed approximately4Hz live attacks, continuous held C4, native looping starter playback/Hear Practice, native Examples approximately every3.75s and137BPM sixteenth clicks. Callback sampleFrame933248→87343360; underflows0,overload0,voiceRefusals0; retained live ownership checked throughout and at end. Native snapshots/events/readings in [soak.json](verification/soak.json). No audible capture. Copied soak executable SHA25610D5821E76FD5A6F64DBA04674BFCDC950FE9894677BC23C812369EA898A16CF differs from final packaged7A1E46F95DFE5C305FA127F07CB6963AE496BB78ECEA95C87A0AE7F83C308C22; subsequent native changes touch validation/window-cache/tray/lifecycle on control threads, with unchanged audio engine/callback/sampler. Final exact packaged binary underwent proportional real WASAPI readiness/playback/browser/restart/shutdown checks. The30minute result certifies that copied audio-core-equivalent binary, not an exact final-binary30minute run.

One-minute actual native process CPU sampling during the mixed load:60606ms,10.390625CPU seconds,17.144% of one logicalcore/~1.0715% of16logicalmachine; working set~492MB (see structured readings). This measures total native process control/sampling/HTTP work, not callback-only CPU or maximum1024-layer pressure. Capacity/no-stealing/owner isolation are exercised by native protection tests separately.

No physical MIDI keyboard, pitch wheel, external instrument or speaker loopback/arrival-to-audible measurement equipment was available. Physical hotplug/readiness/p95 MIDI/virtual key-to-audible and audible30minute dropout certification remain unmeasured. Callback flags, sample-clock continuity, voice refusal and ownership evidence are distinct from acoustic listening/capture. Sample WAV renders are genuine decoded piano, finite/non-silent, with exact internal scheduled event timing; accelerated offline rendering is not the wall-clock soak.

## Portable provenance, licenses and data

Portable folder: build/release/SightReading. Run Start.cmd; quit from the tray or run Stop.cmd. Native executable SHA256: 7A1E46F95DFE5C305FA127F07CB6963AE496BB78ECEA95C87A0AE7F83C308C22. Exact complete release size is recorded in release-manifest.json.

Runtime comprises actual native exe/libc++.dll/libunwind.dll, complete web/dist, unchanged Salamander Samples/Data/protectedSFZ and attribution, five MIDI starters/motifs, bundled notices, WindowsPowerShell start/stop. No Node/compiler/tool/cache/dev dependency ships. PE imports were inspected using llvm-readobj --coff-imports for exe and both DLLs; all other imports are Windows/UCRT system components. Exact file sizes/SHA256/sourceCommit/dirty flag in release-manifest.json. Final seal is repeated after scoped commit, without rebuilding code or changing tracked sources.

Sample author Alexander Holm,CCBY3.0; protectedSFZ informed by kinwie reconstruction, samples unchanged. Starter arrangements independently authored from public-domain compositions, motif contentCC0. Runtime notices include sfizz plus its linked Abseil/jsl/st_audiofile/dr_libs/libaiff/WavPack/stb_vorbis/SIMDe/pugixml/spline/tunings/hiir/kissFFT/cephes/atomic_queue/filesystem/cpuid/threadpool and Faust-generatedDSP/STK/permissiveLGPLexception; PortAudio/RtMidi/json/midifile/SQLite/httplib; LLVMruntime/MinGWruntime; React/ReactDOM/scheduler/VexFlow and Bravura/AcademicoOFL. Compiler and build-only Catch2/npm dependencies are not distributed. Authoritative texts and hashes are in docs/licenses; no reference UNLICENSED code/assets copied.

User data defaults to %LOCALAPPDATA%\SightReading or --data-dir; replacing appfolder retains settings/library/originals/trash. Shutdown/backup/restore described in USER-GUIDE. No system-wide IDE, installer, startup registration or global setting changes. Project tests retained. Tracked ignoredTask6/7 scratchreports are removed from Git only after durable copies; ignored SDDworkspace retained for controller review/cleanup. All fourteen controller rulings with reasons/costs retained verbatim in [rulings.md](verification/rulings.md) and full historical [ledger](verification/implementation-ledger.md); Task1–7 evidence preserved beside it.

Final portable payload before manifest:757869600bytes (~722.76MiB),714files; updated evidence text can change this slightly, authoritative final value is manifest.totalBytes. Active Grand proof shows both clefs/brace/target and keyboard vertically within713px; existing minimum1040px full-keyboard layout retains horizontal octave scrolling at1265px. No claim all88key faces fit simultaneously.
