# Task 7 implementation report

Status: DONE_WITH_CONCERNS (physical hardware coverage only). Base bcea86feab7901ff38b457158350944794665c23. Final SHA supplied in controller handoff because this report is included in the commit.

Implemented native Ear Training/Flash Cards and their pages using existing shared notation, global keyboard, Engine sample clock and reserved Example source125. No dependencies added, sampler changes, reference-project code/assets, Graphify, global installation or remote operations. Original interval-reference musical content is CC0, documented at assets/ear/README.md. Existing attributed Salamander supplies every piano voice.

## Frozen boundaries and behavior

See native/CONTRACT.md for full configs, commands and snapshots. Learn Intervals supports all12qualities, both directions, fixed/random roots, ordered/random question selection, tempo/transposition, root/interval/pair/reference motif, ordered/reverse/harmonic presentation, random-root autoplay, pause/resume/stop/skip. Pair/motif have distinct audible lengths, including harmonic pair vs repeated motif dyads. Melodies support2–8groups ×1–4distinct simultaneous pitches, major fixed/random roots, range presets/custom range, monotonic ascending/descending (repeated groups allowed)/any, replay/new/continuous, opt-in native pitch-wheel shortcuts with center-to-rearm hysteresis.

Note Math uses selected letter roots and inclusive diatonic distances1–7; third of C is E. Seven letters have number1–7 answer shortcuts, avoiding piano keys and ignoring typing/modifiers/repeats. Chord Identification uses selected native major-key diatonic triads, spelled accidentals, inversions, three staff modes and root/accidental/major/minor/diminished answers. Both exercises show feedback/counters/accuracy and reject empty/incompatible settings atomically. Settings persist in SQLite; transient exercise state does not restart.

Every visible route control is connected to native state/commands. Selection checkboxes use atomic native option commands. Example answer locks also apply to Flash after navigation, without suppressing Live audio. Hardware monitoring/global metronome are untouched. Native physical melody scoring matches Staff fresh-owner/pressed-pitch rules and inclusive150ms chord window; sustain, duplicate owners, released/example-held notes cannot answer. Wrong/incomplete groups permit retry. Interval study does not score arbitrary Live notes.

Service composes exercises behind existing bounded input capture; no audio-hook allocation/JSON/scoring/storage. A phrase is bounded32notes/64events; worker submits max128events/pump within quarter-second horizon and checks submissions. Disconnect targets Example125 generation only. Failed cancellation uses existing source emergency recovery; initial example queue failure returns error, later pump failure publishes nonplaying/error/autoplayfalse and keeps worker alive. Pitch-wheel events preserve queue/scheduler release reserve and never trigger Live source recovery. Live/Example still have null channel-gain curves; Song126/Accompaniment127 are not reused.

## Fail-first evidence

From repository root, build command `./scripts/build-native.ps1`; focused runs require portable LLVMbin prepended to PATH. All meaningful native/tests/ear_flash.cpp and tests/ear-flash.mjs retained.

- Initial native `[ear-flash]`: build/task-7-red.log,4cases fail on missing exercise/generator behavior. GREEN task-7-green.log,4219assertions/4cases. Tests enumerate all melody sizes/directions/chord widths and deterministic interval ordering/transposition/root/selection, NoteMath and Gb spelling/inversions.
- `[ear-flash-edge]`: task-7-edge-red.log autoplay root variation failed roots.size1. GREEN native randomized root cycles plus cancellation; task-7-edge-green.log.
- `[ear-flash-held]`: task-7-held-red.log released attacks incorrectly formed chord (hit1 expected0). Owner-key state/fresh attacks fixes; same GREEN file4228assertions/7cases.
- `[ear-flash-wheel]`: task-7-wheel-red.log opt-in shortcut did not play. Native pitch-wheel routing/hysteresis GREEN4assertions.
- `[ear-flash-pressure]`: task-7-pressure-red.log wheel consumed release reserve (25th accepted). Attack capacity and no Live recovery on wheel failure GREEN36assertions.
- `[ear-flash-motif]`: task-7-motif-red.log harmonic motif collapsed to2notes expected4. Repeated harmonic dyad phrase fixed.
- `[ear-flash-lock]`: task-7-lock-red.log Flash answer accepted during Ear example. Shared native answer lock fixed.
- `[ear-flash-example-pressure]`: task-7-example-pressure-red.log saturated3072attacks+1024release queue let replay return success. Startup now errors, scoped recovery preserves existing Live owner; pump callbacks catch errors.
- `[ear-flash-study]`: task-7-study-red.log interval study counted unrelated live note as miss1 expected0. Physical evaluator gated to current unanswered melody only.
- UI/actualservice RED task-7-ui-red.log: installedEdge timeout waiting missing Play root button. EarFlash pages/APIs GREEN task-7-ui-green.log. Initial sandbox test could not atomically rename starter originals; normal Windows permission run demonstrated the intended missing-UI failure. No code bypass added.
- Final focused command `./build/native/native/native_tests.exe '[ear-flash],[ear-flash-edge],[ear-flash-held],[ear-flash-wheel],[ear-flash-pressure],[ear-flash-motif],[ear-flash-lock],[ear-flash-audio],[ear-flash-example-pressure],[ear-flash-study]'` —8383assertions/14cases PASS, build/task-7-focused-green.log. A trial wildcard tag matched no tests; corrected to explicit tags, no false success claim.

## Verification and audio evidence

`./scripts/build-native.ps1` PASS, build/task-7-final-build.log. Web `npm.cmd run typecheck`, `npm.cmd test`, `npm.cmd run build` PASS12tests/5files, build/task-7-final-{typecheck,web,web-build}.log. Existing >500kB bundle advisory remains.

Portable LLVM DLL PATH, normal Windows permissions for native file transactions:

- `.tools/cmake-4.4.4-windows-x86_64/bin/ctest.exe --test-dir build/native --output-on-failure` PASS4/4 in11.23seconds; final log build/task-7-final-native.log.
- `node tests/ear-flash.mjs` PASS actual API/installedEdge: all intervalqualities/directions, transpose, root/interval/motif/forward/reverse/harmonic examples, pause/resume/stop/skip/autoplay, all84melody size/direction combinations, continuous answers, all49NoteMathroot/distance pairs, all36key/inversion combinations, empty settings, heldLive/globalclicks, keyboardshortcuts/M, mobile screenshot/reload and restart persistence. build/task-7-final-integration.log. Screenshot build/task-7-mobile.png ignored evidence.
- Existing `node tests/service.mjs`, `node tests/browser.mjs`, `node tests/keyboard-notation.mjs`, `node tests/staff-practice.mjs`, `node tests/play-along.mjs` allPASS, corresponding build/task-7-final-{service,browser,keyboard,staff,play}.log. These completed after original native integration, before the final isolated queue-error/interval-scoring guards; final CTest/new integration rerun verifies those guards without duplicating unaffected fullbrowser flows.
- `[ear-flash-audio]` renders the attributed real Salamander fixture synchronously through Engine: audible Example energy>0.01, two applied attack frames exactly match native scheduled snapshot (256 and9856 at300BPM/48kHz). Playback channel volume0 does not mute Example. Example stop preserves Live instance900 and running metronome. This is actual offline sampled audio proof, not a speaker/latency claim.

## Preview, cleanup and limitations

Controller visual review uses copied build/task-7-preview/sightreading.exe, isolated build/task-7-preview/data, port18778, PID3248. Controller CUA review confirmed interval-pair continuous Grand notation/accidentals/answer lock, NoteMath fourth of E=A feedback/100%, and Db major ChordID spelled triad/answer controls. No blocker; initial expanded settings push controls below the fold at1265×713, accepted Task8 compact polish carry. Temporary controller tab closed. Exact PID/executable and resolved owned directory verified, preview process stopped and build/task-7-preview removed. Integration suites clean their owned child processes/temporary databases. Logs/audio proofs/screenshots remain ignored; project tests remain tracked.

No physical MIDI keyboard/wheel, external instrument, speaker loopback, MIDI-to-audible latency, exclusive device, or long-session hardware soak was measured. All hardware coverage limitations remain explicit acceptance work. No claims of physical device verification or hardware latency. No new dependency/license obligations; reference motifs are independently authored musical content.

# Review fix round 1

Base8f2ddc5d85321a2720c7a03e438d2f8fee0452bf. Two Important findings and related Shift shortcut Minor addressed; zero open in this round. Scoped final SHA is provided in the controller handoff because this tracked report is included in the commit. Unrelated compact-layout/bundle polish remains Task8.

1. Reverse/harmonic presentation now applies only in interval mode. A selected Learn Intervals presentation persists for return to that exercise, while switching to Melodies always schedules the generated groups in exactly their scoring order/grouping. Transition tests cover BOTH inherited reverse and harmonic choices, compare native scheduled note groups to scoring targets, then answer those scheduled groups successfully.
2. Shared updateExample(nativeFrame) resolves native example scheduling/completion before fresh physical On ownership/lock checks and ear/flash text answer lock checks, as well as the normal worker pump. It does not call general scoring tick or change global Live/metronome. Deterministic tests cover exact endFrame and after endFrame BEFORE a worker tick, pre-end negative and released/fresh boundary attacks, held-before/duplicate-owner suppression, autoplay transition into another locked example, text Ear/Flash answers at the boundary, and paused lock remaining intact.
3. Number-answer shortcuts now ignore Shift. The actual Edge test dispatches a genuine Shift-modified digit event with key3 and checks native answered/hits/misses stay unchanged; ordinary key3 subsequently answers successfully. It catches layouts/numpad cases where a shifted event still reports a digit.

Fail-first evidence from repository root (portable LLVM DLL bin prepended to PATH):

- `./scripts/build-native.ps1` → build/task-7-review-red-build.log, then `./build/native/native/native_tests.exe '[ear-flash-review-order],[ear-flash-review-end]'` → build/task-7-review-red.log:3cases failed,6assertions. Reversed scheduled groups differed from scorer targets; first fresh end-frame attack yielded hits0 instead of1; autoplay endFrame failed to advance at the physical boundary.
- `./scripts/build-native.ps1` → build/task-7-review-command-red-build.log, then `./build/native/native/native_tests.exe '[ear-flash-review-command-end]'` → build/task-7-review-command-red.log:1case failed,3assertions. Flash answer exactly at endFrame incorrectly threw "Answers are disabled during examples."
- Normal Windows permissions `node tests/ear-flash.mjs` → build/task-7-review-shortcut-red.log:Shift-modified digit incorrectly set answeredtrue rather thanfalse.
- After the scoped fixes: `./scripts/build-native.ps1` PASS build/task-7-review-green-build.log; new native tags `[ear-flash-review-order],[ear-flash-review-end],[ear-flash-review-command-end]` PASS25assertions/4cases in build/task-7-review-green.log.
- Final proportional native command `./build/native/native/native_tests.exe '[ear-flash],[ear-flash-edge],[ear-flash-held],[ear-flash-wheel],[ear-flash-pressure],[ear-flash-motif],[ear-flash-lock],[ear-flash-audio],[ear-flash-example-pressure],[ear-flash-study],[ear-flash-review-order],[ear-flash-review-end],[ear-flash-review-command-end]'` PASS8408assertions/18cases, build/task-7-review-final-native.log. Includes all retained Ear/Flash real-sample/native-frame/Live-and-click isolation/queue-pressure cases.
- Web `npm.cmd run typecheck`, `npm.cmd test`, `npm.cmd run build` PASS12tests/5files; build/task-7-review-{typecheck,web-tests,web-build}.log. Existing >500kB bundle advisory unchanged.
- Normal Windows permissions `node tests/ear-flash.mjs` PASS actual native API+installedEdge all Ear/Flash options/answers/examples/Live+click continuity/mobile/reload/persistence plus modified shortcut regression, build/task-7-review-integration.log. Owned child process and temporary database cleaned by the suite. No new preview started.

Native CONTRACT documents the corrected mode-only presentation and shared native completion boundary. No new dependencies, ignored scratch tracking, agents, hardware claims, global installs, Graphify, remote operations, Live/metronome cancellation, or unrelated product changes. Existing tracked Task7 report retained as instructed; physical hardware coverage limitations remain unchanged. git diff --check verified before the scoped commit.
