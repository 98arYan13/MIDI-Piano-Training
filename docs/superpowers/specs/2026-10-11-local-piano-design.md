# Local Piano Practice App — Approved Design

Approved in chat on 2026-10-11. Execution: subagent-driven development, exclusively gpt-6.1-sol, low reasoning. This spec incorporates the user's global metronome, always-on MIDI and protected live-note additions.

## Product and boundaries

Windows 11 x64 app, local browser interface, native C++ service. Five tabs: Staff, Ear Training, Flash Cards, Play Along, Guide. No login, accounts, cloud sync, public uploads, song editor or recording. Independently implemented code and original UI, inspired by sightreading.training. Reference source declares UNLICENSED. Imported music uses scrolling practice notation, not reconstructed conventional scores. All sounds must come from C++, not browser WebAudio.

Stack: C++20/CMake, PortAudio WASAPI, RtMidi, sfizz/Salamander SFZ piano, Craig Sapp midifile, cpp-httplib, nlohmann JSON and SQLite; React/TypeScript/Vite/VexFlow SVG frontend. Catch2 native tests, Vitest frontend tests, Playwright end-to-end. Portable LLVM-MinGW may replace absent MSVC on the current machine; release build remains Windows native. Pin dependencies. sfizz is archived; retain its tested revision and targeted allocator patch.

## Global audio

Hardware input routes directly to native protected piano before exercise scoring and browser events. All available MIDI inputs auto-connect; explicitly disabled inputs remain disabled. Discover on startup and poll every 250 ms. Target readiness within one second of an available device. Connect/reconnect without dialog interaction. Keep device stream open while service runs. Always audible on all tabs, dialogs, fullscreen, paused exercises and example playback. Closing/disconnecting browser releases only its virtual notes; physical monitoring remains.

Persistent toolbar outside routes: connection/status, device setup, monitoring/volume, metronome controls/settings, output status and explicit Panic. Settings accessible above other dialogs and in fullscreen; keyboard shortcut. Native state survives route changes and browser reloads.

Separate protected live, playback/accompaniment, exercise-example and metronome buses. Notes own source/channel/instance IDs. Stops/seeks/resets affect only their owner; only explicit Panic clears all. Initial live pool 1024 sampler voices (sample layers count). Reserve release/pedal voices. Disable live note_selfmask, per-note caps/chokes and live allocator voice stealing. Never rely on approximate active count alone. Reject new voices when exhausted; retain current sound and note-off processing, expose overload. Other buses cannot borrow protected live voices. Float mix, headroom and limiter without ducking.

Audio rebuilds are deferred while live keys or sustain remain; explicit Apply now may interrupt. Panic/disconnection recovery is scoped. No app-level allocation, locks, disk, SQLite, JSON, network or logging in audio callback. Preallocated bounded event queues reserve note-off capacity; reject attacks on pressure. Use native sample clock, workers for sample streaming, and preloaded sample attacks before readiness. No synthesized placeholder piano; use realistic multi-velocity samples with release/pedal/resonance where available, attributed CC BY 3.0 Salamander assets and compatible application SFZ mapping.

Prefer 48 kHz, initially request 128 frames, display actual negotiated WASAPI format/latency; default shared, optional exclusive. Live velocity curve, CC64 sustain, MIDI channel/input filters, internal/external/silent output, native MIDI monitor. Playback channel samplers cannot release live voices.

Measured wired Windows targets: native MIDI arrival-to-audible p95 <=20 ms; virtual key-to-audible p95 <=30 ms; scheduled events within one sample internally; no audible dropout in documented 30-minute load test; UI <=2 s and cold piano ready <=10 s on documented test machine. Do not claim hardware measurements without an actual device. Overload refuses new allocations, never unlimited polyphony claims.

## Global metronome

Engine-owned running state on all tabs including Guide/Flash Cards, coexists with all other sounds and survives navigation/dialogs/fullscreen/reload. Piano/song/example stop does not stop it; click stop does not affect piano. Default Independent; optional Follow practice/song links tempo/meter/position. Transport stop leaves clicks running at last tempo. Explicit metronome stop/Panic/service shutdown ends it. Tempo change next beat; subdivision/meter next bar; no audio restart. Native sample-clock clicks with preloaded original sounds.

Tempo 20–300 BPM, numeric/slider/tap tempo. Meter numerator 1–12, denominator 2/4/8/16; clear beat unit including compound time. Quarter/eighth/sixteenth/triplet subdivisions; strong/weak/mute per beat; swing 50–75%; count-in 0–4 bars; click/woodblock/rim sounds; independent volume/mute. Default stopped, independent, 100 BPM 4/4 quarters first accent, one-bar count-in.

## Staff and keyboard

Treble/Bass/Grand/chord-symbol mode. Grand joined bass+treble and full 88-key keyboard available; exercise range independent. MIDI 60=C4; piano A0–C8. Random 1–5 notes, 1/2 hands, chord based; Triads inversions; Sevens open voicing; Progression Autumn Leaves/50s/circle/basic substitution; Position five-finger starting fingering; Intervals selected diatonic seconds–sevenths. Smoothness 0–6; min/max range; key C/G/D/A/E/B/F/Bb/Eb/Ab/Db/Gb and Chromatic. Chord-symbol random/multi-key, major/natural/harmonic/melodic minor, 3/4 tones, common-note any/1/2 and ignore-above. Seeded bounded generator search; actionable invalid/empty configuration errors.

Wait advances on correct pressed notes, 150 ms chord window; wrong notes count misses and allow retry. Scroll advances native tempo, default timing tolerance +/-150 ms, adjustable. Physical key state, not sustained sound, determines score. Counters hits/misses/accuracy/elapsed, reset/fullscreen. Default treble/random/C major, 1 note, 2 hands, smoothness 3, Wait. Settings changes affect exercise only.

Virtual keyboard mouse/multitouch/computer keys/chords, velocity, sustain display, separate live/playback colors, full/focused range, octave navigation, labels and hide. Pointer cancel/focus loss releases virtual only. Smooth cached SVG notation and requestAnimationFrame transform scrolling, responsive full keyboard with horizontal pan where necessary, accessible controls and reduced motion.

## Ear Training / Flash Cards

Learn Intervals: interval quality/direction selection, play root/interval/reference motif, next/random, tempo/transposition, autoplay random roots, ordered/reverse/harmonic, pause/stop/skip. Original/licensed motifs. Play Back Melodies: 2–8 groups, 1–4 simultaneous pitches, range presets, fixed/random major root, ascending/descending/any, replay/new/continuous, optional pitch-wheel shortcuts. Answers disabled during examples but live sound always remains.

Note Math: selected C D E F G A B roots, diatonic letter counting, seven answers/shortcuts (third of C=E). Chord Identification: staff, selected keys, inversions, root/accidental/major/minor/diminished answers. Feedback/session statistics, empty settings message. Global metronome and MIDI work here.

## MIDI library / Play Along

Upload .mid/.midi or drop; preserve originals in local library. Titles/count/duration/tracks/recent. Independently prepare licensed/public-domain arrangements: Twinkle Twinkle, Mary Had a Little Lamb, Ode to Joy, Old Dan Tucker, BWV846 short excerpt. Play/pause/stop/restart/seek, speed/tempo, A–B loops, spacing; practice/backing track selection; continuous timing or pause-on-miss, mute practice part by default, option to hear it. Scrolling pitches/accidentals/note-duration indicators/playhead and staff assignments with pitch split fallback.

SMF0/1 PPQ, preserve tempo/meter/velocity/tick/channel/controller data. Missing defaults 120 BPM 4/4. Pitched tracks piano, percussion detected/disabled default. Reject SMF2/SMPTE clearly in v1. Limits 20 MB, 1 million events, 256 tracks, 2 hours; malformed/empty rejected atomically. Accurate overlapping same pitch source ownership. Seek/loop restores channel controller state and spanning notes on playback bus only.

Auto accompaniment Off/Root/Triad/Root+5/Arp/Bossa Nova, spacing/rate. Explicit starter chord maps; infer duration-weighted pitch classes per measure for imports, major/minor/diminished/seventh templates >=70% weighted coverage; uncertain bars silent. Preserve original tracks regardless inference.

Guide accurately documents features, installation/device setup, global audio and metronome, shortcuts, imports, loops, limits, tuning and troubleshooting.

## Protocol / persistence / packaging

Native music model NoteEvent{id,trackId,channel,midiNote,velocity,startTick,endTick}; Song{id,title,ppq,tracks,notes,controllers,tempoMap,meterMap,keyMap}; PracticeConfig{staff,generator,key,range,generatorOptions,mode,tempo,toleranceMs}; GlobalAudioState{readiness,devices,monitoring,volumes,heldNotes,overload,pendingChange}; MetronomeState{running,mode,tempo,beatUnit,meter,subdivision,accents,swing,countIn,sound,volume}; TransportSnapshot{generation,sampleFrame,sampleRate,engineTimeNs,songTick,state,speed}. Pitch spelling separate from pitch, preserve ticks.

HTTP /api/v1/status, /devices, GET/PUT /settings, GET /metronome, GET /songs, POST /songs/import multipart, GET /songs/{id}, GET /songs/{id}/events windowed, PATCH /songs/{id} title/assignments, DELETE recoverable trash. WebSocket /api/v1/events versioned JSON envelopes with request IDs, commands for virtual on/off/pedal/Panic/devices/exercises/playback/metronome. Push note state, status, devices, exercise/results, beat events and 30 Hz transport snapshots. Generation prevents stale updates; clock ping/offset and browser interpolation, never JS audio scheduling.

One loopback origin 127.0.0.1:8765, explicit --port, strict Host/Origin, no wildcard CORS. Static self-hosted assets/no CDNs. SQLite transactions/schema versions and %LOCALAPPDATA%/SightReading data (or --data-dir for tests). Launcher/service/tray shutdown, offline installation without Node runtime, retain user library across upgrade/reinstall.

## Release acceptance

Verify every control, all 5 tabs offline, Grand two staves/full keyboard, MIDI import/playback/loop/score, always-on live sound, global clicks, all-key dense-load protection, capacity refusal, overlapping source notes, pedal/repeated-note tails, isolated stops and reload, scoped disconnect, deferral of reconfiguration, >=30 min soak, latency/readiness/browser responsiveness on actual available hardware. Document unverified hardware explicitly. Target 60fps cached notation, dense 10-minute song without sustained stalls. Retain meaningful project tests.
