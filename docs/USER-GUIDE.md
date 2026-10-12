# Sight Reading user guide

## Install & start

Unzip or copy the complete portable SightReading folder to a writable local directory, then run Start.cmd. Windows 11 x64 and an installed Edge/Chrome browser are sufficient; Node, compiler tools and internet are unnecessary. Open http://127.0.0.1:8765. Keep all piano Samples, runtime DLLs and web files together. Quit from the notification-area icon or Stop.cmd; closing the browser intentionally leaves native MIDI and clicks running.

## Connect & listen

Connect a USB MIDI keyboard and choose Audio / MIDI. Enabled inputs are discovered every 250 ms and opened automatically. Choose the WASAPI output, internal sampled piano / external MIDI instrument / silent monitoring, input channels, velocity curve and volume. Monitoring is global on all five tabs, dialogs and fullscreen. CC64 controls sustain. Device/exclusive changes wait for live keys and sustain to release; Apply now interrupts live ownership. Panic explicitly clears every bus and stops clicks.

## Keyboard & shortcuts

A0–C8 covers all 88 keys; C4 is MIDI 60. Use mouse, touch or A W S E D F T G Y H U J K O L P ; from the selected C octave. Space holds virtual sustain. Full/focused range, octave, labels, velocity and hide controls do not change exercise range. Losing browser focus releases virtual notes only. Green = live, blue = playback, gold = examples; hatching = sustained. M opens global metronome settings outside typing fields. Fullscreen keeps the toolbar accessible. Flash number keys 1–7 answer C D E F G A B; modifiers/repeats/typing are ignored.

## Global metronome

Independent is the default. Follow links practice/song tempo and musical position; stopping transport leaves clicks at the last tempo. Quarter-note BPM 20–300; meter 1–12 over 2/4/8/16. Denominator is the beat unit: 6/8 has six eighth-note beats. Quarter/eighth/sixteenth/triplet labels mean 1/2/4/3 pulses per denominator beat. Tempo changes next beat; meter/subdivision/swing next bar; count-in 0–4 bars on the next start. Tap tempo, strong/weak/mute accents, swing 50–75%, three original sounds, volume and mute remain available globally.

## Staff practice

Select Treble, Bass, joined Grand or chord symbols. Native seeded generators offer random 1–5 notes, one/two hands, chord-based targets, triad inversions, open sevenths, Autumn Leaves/50s/circle/substitution progressions, fixed five-finger positions and selected diatonic intervals. Choose range, smoothness 0–6, named key or Chromatic and major/natural/harmonic/ascending melodic minor. Chord symbols support multiple keys, 3/4 tones and common-note constraints. Invalid combinations retain the previous valid settings and explain the problem. Start/Reset creates 64 targets; Reset clears exercise score/time without releasing live notes.

## Wait, Scroll & scoring

Staff shares Play Along's measure-based engraving: numbered four-beat measures, key signatures, bar-based accidentals, aligned chords and collision-aware spacing. Native exercise spelling and timing are preserved. Notes move through a thin centered orange marker. The orange play button starts an exercise, pauses it, or resumes from the same position and score; paused time is excluded from elapsed time. Restart exercise in the toolbar returns to target one and clears score/time. Left and right hand buttons toggle the practiced hands, keeping at least one enabled: left uses notes below middle C (MIDI 60), right uses middle C and above. Selecting one hand switches to its clef; selecting both switches to Grand. Hand selection keeps your configured range and stops only the exercise; an incompatible range is rejected with an error. The separate one/two-hands generator option controls chord span when both hands are enabled.

Wait needs the exact physically held target pitches with fresh attacks within 150 ms. Wrong attacks count misses and allow retry after release. Scroll uses native quarter-note timing with adjustable ± tolerance (default 150 ms). Its target tolerance is independent of the fixed 150 ms chord-assembly window. Sustain, playback and examples cannot answer. Held-before-start notes require new attacks. Hits, misses, accuracy and elapsed time are native-authoritative; stopping/settings edits affect only the exercise.

## Ear Training

Learn Intervals selects qualities and up/down directions, root/random root, transposition, tempo, ordered/random sequence and forward/reverse/harmonic audition. Play root, interval or an original reference motif; Next/Random/Skip, Pause/Stop and randomized-root Autoplay control only examples. Play Back Melodies generates 2–8 groups of 1–4 pitches in the selected range/key/direction. Replay/New/continuous mode and optional wheel shortcuts are available. A wheel high edge makes a new question, low replays, and returning to center rearms; it does not bend the piano. Melody answers use fresh physical attacks after examples, exact pitch sets and a 150 ms chord window. Sound always monitors while answers are locked.

## Flash Cards

Note Math selects C–B roots and diatonic distances 1–7, counting the root as one: third of C is E. Answer with letters or unmodified number shortcuts. Chord Identification selects keys/inversions and Treble/Bass/Grand, then asks for root, natural/flat/sharp and major/minor/diminished quality. Wrong answers permit retry, correct answers require Next. Reset clears session statistics. Answers are locked during Ear examples or pause, even across navigation.

## Play Along & library

Wait targets and hints include the selected hand's notes from all enabled pitched tracks, including tracks marked only as Backing. You do not need to reassign the backing track to practice a left or right hand. Tracks with both Practice and Backing off remain excluded. Ordinary timing scores continue to use the tracks marked Practice.

Turn on **Wait mode** to hold the score at each target until you press the correct note or chord for the selected left, right, or both hands. Waiting itself does not count as a miss. Turn Wait off to continue with normal timing. **Hint mode** independently highlights target keys on the piano below: teal for left hand, orange for right hand, and a split color when both hands share a key. Hints work with Wait on or off and do not play notes for you.

Drag the score left to move forward, or right to move backward. The center marker stays fixed. Dragging pauses playback and leaves the song at the chosen position; press Play to continue. The orange position strip updates to match. Dragging stops at the beginning and end of the song.

Play Along reconstructs a practice score from the imported MIDI: complete measures, aligned hands, voices, rests, dotted values, ties and meter-based beams. Score spacing changes engraving width; crowded passages receive more room automatically. Accidentals follow the key signature and reset each measure. Explicit track staff assignments take priority; RH/LH track names guide display hand placement. Copies of the same quantized pitch and duration are printed once. Original MIDI playback and scoring retain every event and its original timing.

MIDI does not encode authoritative written notation. Display timing is rounded to a 32nd-note grid; spelling, voices and rhythms are inferred. Tuplets, grace notes, pickups and expressive articulation may differ from a published score. No quantization is applied to the native playback clock or the stored original.

The player uses the same paper-white score, centered marker and orange play/pause control as Staff. Restart sits on the marker; the orange strip below the score seeks through the song. Numbered measures follow the imported MIDI tempo and meter maps. Hand buttons select the scored staff and dim the other staff: explicit bass/treble track assignments take priority, while split/grand tracks use middle C as the hand boundary. At least one hand stays selected. Hand selection does not alter Backing or Hear practice part, release live keys, or stop the global metronome. It is retained across song changes during the current native session.

Choose one of ten built-in MIDI pieces (five starter arrangements and five original practice exercises), upload or drop .mid/.midi, rename, assign practice/backing tracks and staff, or download the original. Remove moves a song to recoverable trash; Restore returns it. Play/Pause/Stop/Restart/seek and speed/tempo are native-clock controls. Both practice and backing parts play by default; uncheck Hear practice part beside Play to mute the practice part. Use the toolbar menu button to hide/show the navigation sidebar. Continuous timing or Pause on miss uses fresh same-tick pitch groups, independent timing tolerance and 150 ms chord assembly. Set A/B ticks or Set here to loop; choose spacing and Off/Root/Triad/Root+5/Arp/Bossa Nova accompaniment. Rate scales pattern density; voicing spacing chooses close/open. Starter chord maps are explicit; imports infer chords at ≥70% weighted coverage and leave uncertain bars silent.

## Imports & controller limits

SMF types 0/1 with PPQ are supported; type 2/SMPTE are rejected. Limits: 20 MB, one million events, 256 tracks, two hours, cumulative ticks ≤2,147,483,647. Originals retain tempo/meter/key maps, channels, velocities, overlapping notes and raw controllers. Pitched tracks audition as piano; drums are disabled by default. Audible playback mappings: CC64 sustain, CC7 volume, CC11 expression, CC10 pan and CC120/123 release; other controllers and pitch bend are preserved without an audition mapping. Seek/loop restores spanning notes and controller state only on playback. Dense windows cap at 4096 notes and report truncation; accompaniment/restoration capacity errors are explicit and retain live sound/clicks.

## Your data & upgrades

Settings, originals, titles, assignments, recent songs and recoverable trash live in %LOCALAPPDATA%/SightReading (or --data-dir). Quit before backing up the complete directory, including SQLite/WAL files and originals. Upgrade by replacing the application folder while retaining that separate data directory. Running state, transient questions and held notes are not restored after service restart; a browser reload reconnects to the existing native state. The service listens only on 127.0.0.1 with strict same-origin controls.

## Tuning & troubleshooting

Start with shared WASAPI at the requested 48 kHz/128 frames; Audio / MIDI reports the actual format and API output latency, which is not measured key-to-audible latency. For unavailable audio check output selection, another application's exclusive access and sample files. Exclusive/reopen failures are shown; Quit/Start releases the device cleanly. Check disabled inputs/channels, monitoring, output mode and volume for missing notes. Capacity refuses new voices rather than stealing existing live sound: 1024 sampler layers, 960 attack slots plus 64 release reserve, separate playback/example/click ownership. Overload and underflow diagnostics are cumulative. Physical MIDI latency/hotplug, external output, wheel and audible dropout certification need actual hardware measurements; see the release verification report for measured coverage.
