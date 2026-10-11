# Local Piano Practice Implementation Plan

> For agentic workers: use superpowers:subagent-driven-development task by task. Every agent uses gpt-6.1-sol with low reasoning; no escalation or worker-spawned agents.

**Goal:** Implement all requirements in the approved design, including always-on MIDI, global clicks and protected live voices.
**Architecture:** Single native Windows C++ service serves a React frontend and owns hardware, sample clock, training and local data. Sound buses have independent ownership. Browser visualizes engine events, never schedules audio.
**Tech stack:** C++20, CMake, LLVM-MinGW or MSVC, PortAudio/RtMidi/sfizz/midifile/cpp-httplib/nlohmann-json/SQLite, React/TypeScript/Vite/VexFlow, Catch2/Vitest/Playwright.
**Spec:** docs/superpowers/specs/2026-10-11-local-piano-design.md

## Global constraints

- Read spec; all its requirements are binding. No placeholders passed off as completion.
- All workers/reviewers/fixers exclusively gpt-6.1-sol, low reasoning; no agents inside workers.
- All musical sound and hardware in C++; no browser WebAudio substitution.
- Live notes and metronome remain across tab/dialog/transport changes. Only their owner stops them; Panic explicit.
- Independent code and UI; reference UNLICENSED; attributed redistributable sample assets.
- Native Windows executable; offline installed runtime; project-local compiler/build assets excluded from Git.
- No graphify update. Retain project tests, remove only disposable diagnostics after use.

## Review focus

1. Source/channel ownership at same pitch; unrelated note-offs must not stop it (Tasks 1,6).
2. Resource exhaustion and sustain/release sample layers; old live voices retained (Task 1).
3. Reconfiguration/browser disconnect/page changes must not reset hardware/metro (Tasks 1–4).
4. Malformed tempo maps/imports and seek/loop controllers preserve original events (Task 6).
5. Native clock vs UI timeline; no timer-driven sound or blocked callback (Tasks 2–7).

### Task 1: Protected native piano foundation

**Files/ownership:** native/, root CMakeLists.txt/CMakePresets.json, scripts/setup-native.ps1/build-native.ps1, assets/piano/, dependency pin manifest. Produce native audio/MIDI components and their C++ tests. Do not edit frontend files.
**Interfaces:** Produce `native/include/sightreading/Engine.hpp` with `Engine` native controller usable by later service. `nlohmann::json command(const nlohmann::json&)`, `nlohmann::json snapshot() const`, `void start()`, `void stop()`, `void render(float*, unsigned frames)` test/offline path, component types/hooks as needed. Native Engine command API and snapshot keys documented in native/CONTRACT.md; versioned request envelope {version:1,id,type,params}, reply {version:1,id,ok,result,error}; stable error {code,message}. Engine status includes audioReady/pianoReady/error/devices/current held notes. Later tasks add commands/buses without resetting live ownership.
- [ ] Write Catch2 failing behavior tests for sustained C4 surviving many other keys, repeated tails, virtual/native same-pitch ownership, source-scoped off, protected admission at capacity, release reserve, virtual disconnect and deferred changes. Exercise real voice allocator/render scheduling, not mock call assertions.
- [ ] Provision reproducible project-local native compiler/dependencies from official sources. Build real PortAudio WASAPI, RtMidi and sfizz, install/download attributed piano samples. Pin source revisions and setup asset checksums. Source sampler real sampled piano, not a sine fallback. Expose accurate not-ready failures.
- [ ] Implement separate live/playback/example/click ownership. Protected live 1024 voices and no stealing; instrument mapping eliminates live choke/selfmask/single-note caps; app-owned targeted sampler allocation change if stock insufficient. Bounded queues reserve release events, no callback app-level allocations/locks/IO.
- [ ] MIDI auto-enumerate250ms/open/reconnect; live monitoring always. 48kHz/128-frame request and actual WASAPI status; sample attacks preloaded, worker streaming; velocity/CC64/filters/external/silent/Panic/deferral. Headroom+limiter.
- [ ] Run focused red→green then CTest full suite; render sample audio to WAV for 88-key/velocity/pedal evidence and inspect peaks/non-silence. Document unavailable physical MIDI/loopback measurements. Commit report status/tests/risks/contract paths.

### Task 2: Global metronome and mixer timing

**Files/ownership:** native metronome/transport/mixer components, C++ tests; native CONTRACT additions.
**Consumes:** Engine from Task1 and render/sample-clock hooks. **Produces:** metronome.config/start/stop commands; MetronomeState from spec, beat events and independent click audio.
- [ ] Failing sample-render tests: 100BPM48kHz beat positions 0/28800/57600 after startup/count-in policy; simultaneous piano+click non-silent; stopping one bus leaves other; next-beat tempo/next-bar meter edits; third/eighth/sixteenth/swing accent and all-mute.
- [ ] Implement global independent running clock, Follow transport adapter that continues at last tempo on transport stop, 20–300BPM/meter1–12/denom2,4,8,16/subdivisions/swing50–75/count0–4/accent pattern/3 original sounds/volume/mute. No callback reset/restart on changes. Persist settings separately from runtime running state.
- [ ] Verify focused tests and full native suite; rendered audio alignment and cancellation; commit/report contract.

### Task 3: Local service, persistence and browser shell

**Files/ownership:** native server/storage/main components, scripts/start.ps1, web shell/types/native client/global store, root app scripts and README.
**Consumes:** Engine command/snapshot, metronome. **Produces:** HTTP/WebSocket API and global React provider outside routes; app nav and fixed global toolbar. Freeze shared web/native payload types in web/src/types.ts and native/CONTRACT.md.
- [ ] Failing boundary tests for strict origins, invalid JSON/unknown commands, state restoration, persistence and WebSocket connect/reconnect without mutating global playing state.
- [ ] Serve built assets at loopback8765(--port,--data-dir), API status/devices/settings/metronome, songs routes foundations; sqlite migrations and atomic changes; native process continues if browser goes away. JSON command envelope above; streamed engine snapshots30Hz and note/beat events; UI clock estimation/generation filtering.
- [ ] React dark polished responsive app shell, five routes, accessible persistent MIDI/audio/metronome/Panic controls, global configuration dialogs over route dialogs and fullscreen; shared state from native only. Piano not-ready progress/errors truthful. No browser sound fallback, no cloud assets. Keyboard shortcut M opens metro settings (except typing input).
- [ ] Install local npm dependencies; typecheck/unit/build and native API integration; offline runtime startup and browser reload; commit/report.

### Task 4: Polyphonic keyboard and scrolling notation

**Files/ownership:** web keyboard/notation components and related UI tests, native virtual-input commands if needed.
**Consumes:** shared native client/types, global piano input/state. **Produces:** reusable 88-key keyboard and SVG practice staff with timeline/event highlights.
- [ ] Failing behavior tests C4=60, Grand two clefs+88keys, pointer/key chord ownership, cancellation scoped virtual, focus loss hardware untouched and resize geometry.
- [ ] VexFlow cached SVG segments, joined grand staves, pitch spelling separate from MIDI, accidents/ledger lines/duration bars/fixed playhead, RAF transforms synchronized native clock; reduced-motion support. Key range/focused/full/octave/labels/velocity/hide/settings, mouse/multitouch/computer map and native/sequence colors.
- [ ] Render visual quality on real browser; Playwright responsive/fullscreen/multiple held-key controls; full web tests/typecheck/build and relevant native tests; commit/report.

### Task 5: Staff generation and scoring

**Files/ownership:** native music theory/generator/practice components and tests, web Staff settings/exercise UI.
**Consumes:** shared keyboard/staff/native clock/events. **Produces:** practice.configure/start/reset/answer commands, target/result events and PracticeConfig schema.
- [ ] Failing deterministic theory/generator/scoring tests for all spec keys/modes/options, triad/seventh/interval spelling, hand span/smoothness/five-finger behavior, range rejection, empty interval toggle, chord-class multi-key/common notes, timing/chord window and score reset not live reset.
- [ ] Implement every Staff control in spec, seeded bounded generation and actionable incompatible setting errors, Wait/Scroll native clock and physical state scoring; hits/misses/accuracy/elapsed, sliders/key/range controls and fullscreen. Notes arriving during examples still monitor even if evaluator inactive.
- [ ] Full native/web suites, Playwright Grand/settings/answers/reset with held keys; commit/report.

### Task 6: MIDI library, Play Along and accompaniment

**Files/ownership:** native SMF import/song/transport/accompaniment/storage integration/tests; web library/Play Along; assets/music starters/licenses.
**Consumes:** Task1 buses, Task2 sample clock/Follow, Task3 API/storage, Task4 staff/key, Task5 theory/scoring. **Produces:** complete song routes from spec, transport commands and canonical Song/NoteEvent/TransportSnapshot, windowed notation.
- [ ] Failing real binary MIDI fixtures type0/1/tempo/meter/overlap/control0-velocity noteoff; malformed/empty/size/type2/SMPTE rejection; atomic storage and recycle; seek/loop spanning/controller restore; training/backing ownership and metronome independence.
- [ ] Use midifile preserve timing/tracks/channel controllers/key/time metadata, limits20MB/1m events/256tracks/2h; default120BPM4/4; track staff split, piano map/drum off. Original files+SQLite library, imports/delete recoverable trash/title/assignments/recent.
- [ ] Implement real native play/pause/seek/speed/A–B/restart/track selection/hear-practice/continuous+pause-on-miss. Autochords Off/Root/Triad/Root+5/Arp/Bossa, spacing/rate, explicit starter chords and70% inference confidence with silence uncertain. Independently authored5licensed MIDI starters and provenance.
- [ ] Real imports API/UI browser exercise; full native/web tests/typecheck/build and playback WAV timing inspection; commit/report.

### Task 7: Ear training and Flash Cards

**Files/ownership:** native exercise engines/theory tests/assets original interval motifs, web Ear/Flash pages/settings.
**Consumes:** native example bus/clock, theory, global controls, keyboard/staff. **Produces:** ear.configure/play/root/interval/melody/next/autoplay/replay/answer and flash.configure/next/answer commands, results/session stats.
- [ ] Failing deterministic interval/transpose/order/root/selection/empty tests; melody2–8groups1–4pitches/range/root/direction/continuous/input suppression during examples (sound remains), scoped replay/autoplay cancellation, noteMaththirdC=E, chord(root/accidental/quality/inversion/key) answer correctness.
- [ ] Every exercise/control in spec incl original licensed motifs/autoplay/pitch-wheel optional, noteMathroots/shortcuts, chordIDstaff and inversions; visible answer feedback/session stats and invalid selection errors. Global hardware/metronome untouched.
- [ ] Full suites/Playwright flow each exercise and metro continuity, commit/report.

### Task 8: Offline release, Guide and integrated validation

**Files/ownership:** web Guide and polish, README/docs/licenses, scripts packaging/setup/test/diagnostic tooling, focused regression fixes via worker.
**Consumes:** complete app. **Produces:** usable project-local Windows native release + frontend/piano assets, documented start/setup/install/stop and limits, reproducible full verification report.
- [ ] Real end-to-end regression covers all5tabs/controls with native API, MIDI fixtures, held-key persistence, metronome global audio continuity, service/browser reconnect, input auto-open and deferred settings. Virtual loopback can test native device routing; actual keyboard measurements only if available.
- [ ] Guide accurate allfeatures, portable offline release noNode, save/restore user data/recoverable removal, self-hosted fonts/assets, startup tray/launcher. Log hardware/code/library/build provenance and actual supported voices/buffer/latency/30min soak limitations.
- [ ] Run cmake build/ctest, npm ci/typecheck/test/test:e2e/build, real browser visual checks and native audio renders. Broad final independent review on same permitted model; implement one consolidated fix wave and scoped review.
- [ ] Report shipped behavior vs measured/unmeasured limitations; no falsely successful hardware claims. Remove only owned scratch artifacts after verification; keep tests and implementation. Commit.
