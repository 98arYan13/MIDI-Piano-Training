# Sight Reading user guide

## Install & start

Unzip or copy the complete portable SightReading folder to a writable local directory, then run Start.cmd. Windows 11 x64 and an installed Edge/Chrome browser are sufficient; Node, compiler tools and internet are unnecessary. Open http://127.0.0.1:8765. Keep all piano Samples, runtime DLLs and web files together. Quit from the notification-area icon or Stop.cmd; closing the browser intentionally leaves native MIDI and clicks running.

## Remembering your workspace & resetting settings

The app saves your last tab, sidebar and Focus mode, keyboard size/range/labels/velocity/octave, audio and MIDI preferences, metronome settings, exercise settings, and Play Along options in the native local database. Opening the app without a tab link returns to your last tab; an explicit tab link takes precedence. The last selected song returns at its saved position, paused. Playback checkpoints are queued once per second and immediately after transport controls. A native background worker saves them without blocking playback; graceful shutdown flushes the final position. An abrupt power loss may lose the last second of progress. Deleted songs are not reopened.

Use **Settings** in the global toolbar, then **Reset all settings to defaults**. This resets display, audio/MIDI, metronome, exercise and playback preferences while keeping the song library, track assignments, selected song and current song position. Live notes remain protected and a running metronome keeps running. Audio changes that could interrupt held notes wait for live keys and sustain to release. Exercise settings reset may create a new exercise; playback configuration changes keep the current song position. The metronome's settings are restored across service restarts, but its running state starts stopped.

## Connect & listen

Connect a USB MIDI keyboard and choose Audio / MIDI. Enabled inputs are discovered every 250 ms and opened automatically. Choose the WASAPI output, internal sampled piano / external MIDI instrument / silent monitoring, input channels, velocity curve and volume. Monitoring is global on all five tabs, dialogs and fullscreen. CC64 controls sustain. Audio-device changes wait for live keys and sustain to release; Apply now interrupts live ownership. Output always uses shared WASAPI so movies and other apps can play alongside the piano, including after restoring older exclusive-mode settings. Maximum piano volume uses full sample gain with a bounded limiter. Panic explicitly clears every bus and stops clicks.

## Keyboard & shortcuts

A0–C8 covers all 88 keys; C4 is MIDI 60. Use mouse, touch or A W S E D F T G Y H U J K O L P ; from the selected C octave. Space holds virtual sustain. Full/focused range, octave, labels, velocity and hide controls do not change exercise range. Losing browser focus releases virtual notes only. Green = live, blue = playback, gold = examples; hatching = sustained. M opens global metronome settings outside typing fields. Fullscreen keeps the toolbar accessible. Flash number keys 1–7 answer C D E F G A B; modifiers/repeats/typing are ignored.

## Global metronome

Independent is the default. Follow links practice/song tempo and musical position; stopping transport leaves clicks at the last tempo. Quarter-note BPM 20–300; meter 1–12 over 2/4/8/16. Denominator is the beat unit: 6/8 has six eighth-note beats. Quarter/eighth/sixteenth/triplet labels mean 1/2/4/3 pulses per denominator beat. Tempo changes next beat; meter/subdivision/swing next bar; count-in 0–4 bars on the next start. Tap tempo, strong/weak/mute accents, swing 50–75%, three original sounds, volume and mute remain available globally.

## Staff practice

Select Treble, Bass, joined Grand or chord symbols. Native seeded generators offer random 1–5 notes, one/two hands, chord-based targets, triad inversions, open sevenths, Autumn Leaves/50s/circle/substitution progressions, fixed five-finger positions and selected diatonic intervals. Choose range, smoothness 0–6, named key or Chromatic and major/natural/harmonic/ascending melodic minor. Chord symbols support multiple keys, 3/4 tones and common-note constraints. Invalid combinations retain the previous valid settings and explain the problem. Start/Reset creates 64 targets; Reset clears exercise score/time without releasing live notes.

## Wait, Scroll & scoring

Staff **Hint mode** highlights the current target note or complete chord on the piano: teal for left hand, orange for right hand. It follows the printed staff assignment, advances with the native target and clears when the exercise finishes. Switching hints preserves exercise position, score and paused/running state. It also works with chord symbols.

Staff shares Play Along's measure-based engraving: numbered four-beat measures, key signatures, bar-based accidentals, aligned chords and collision-aware spacing. Native exercise spelling and timing are preserved. Notes move through a thin centered orange marker. The orange play button starts an exercise, pauses it, or resumes from the same position and score; paused time is excluded from elapsed time. Restart exercise in the toolbar returns to target one and clears score/time. Left and right hand buttons toggle the practiced hands, keeping at least one enabled: left uses notes below middle C (MIDI 60), right uses middle C and above. Selecting one hand switches to its clef; selecting both switches to Grand. Hand selection keeps your configured range and stops only the exercise; an incompatible range is rejected with an error. The separate one/two-hands generator option controls chord span when both hands are enabled.

Wait needs the exact physically held target pitches with fresh attacks within 150 ms. Wrong attacks count misses and allow retry after release. Scroll uses native quarter-note timing with adjustable ± tolerance (default 150 ms). Its target tolerance is independent of the fixed 150 ms chord-assembly window. Sustain, playback and examples cannot answer. Held-before-start notes require new attacks. Hits, misses, accuracy and elapsed time are native-authoritative; stopping/settings edits affect only the exercise.

## Ear Training

Ear Training has its own **Hint mode**. For intervals it reveals the root and interval notes; for Play Back Melodies it reveals only the current answer group and advances when you answer that group correctly. Hints clear after the question is answered and update for the next question. Teal marks pitches below middle C and orange marks middle C and above; these are keyboard location cues rather than prescribed fingerings. Toggling hints leaves native examples, scores, physical monitoring and the global metronome running. Hint preferences are independent for Staff, Ear Training and Play Along and are saved locally.

Learn Intervals selects qualities and up/down directions, root/random root, transposition, tempo, ordered/random sequence and forward/reverse/harmonic audition. Play root, interval or an original reference motif; Next/Random/Skip, Pause/Stop and randomized-root Autoplay control only examples. Play Back Melodies generates 2–8 groups of 1–4 pitches in the selected range/key/direction. Replay/New/continuous mode and optional wheel shortcuts are available. A wheel high edge makes a new question, low replays, and returning to center rearms; it does not bend the piano. Melody answers use fresh physical attacks after examples, exact pitch sets and a 150 ms chord window. Sound always monitors while answers are locked.

## Flash Cards

Note Math selects C–B roots and diatonic distances 1–7, counting the root as one: third of C is E. Answer with letters or unmodified number shortcuts. Chord Identification selects keys/inversions and Treble/Bass/Grand, then asks for root, natural/flat/sharp and major/minor/diminished quality. Wrong answers permit retry, correct answers require Next. Reset clears session statistics. Answers are locked during Ear examples or pause, even across navigation.

## Play Along & library

The guided tutorial collection includes original beginner foundations, Burgmüller Op. 100, Czerny Op. 840/821 and Hanon Part I. In the library, search by title or skill and use Difficulty and Collection to narrow the list. Select a lesson to read its goal, suggested quarter-note BPM, practice steps and tips beside Play Along. Source / score opens the credited edition separately. Start with Foundations, then choose studies suited to your current reading and coordination; Hanon and the harder studies are optional technique work, not a required beginner sequence. See [the tutorial guide](TUTORIALS.md).

For developer checkouts run `node scripts/setup-tutorials.mjs` once, then restart the service to add the lessons. Complete portable releases include the files and require no download. Guides and MIDI playback work offline; source links are online. Existing imported pieces, track assignments, titles and recoverable trash are preserved. The app checks pitch and timing, not fingering, tone or technique.

Wait targets and hints include the selected hand's notes from all enabled pitched tracks, including tracks marked only as Backing. You do not need to reassign the backing track to practice a left or right hand. Tracks with both Practice and Backing off remain excluded. Ordinary timing scores continue to use the tracks marked Practice.

Turn on **Wait mode** to hold the score at each target until you press the correct note or chord for the selected left, right, or both hands. Waiting itself does not count as a miss. Turn Wait off to continue with normal timing. **Hint mode** independently highlights target keys on the piano below: teal for left hand, orange for right hand, and a split color when both hands share a key. Hints work with Wait on or off and do not play notes for you.

Drag the score left to move forward, or right to move backward. The center marker stays fixed. Dragging pauses playback and leaves the song at the chosen position; press Play to continue. The orange position strip updates to match. Dragging stops at the beginning and end of the song.

Play Along reconstructs a practice score from the imported MIDI: complete measures, aligned hands, voices, rests, dotted values, ties and meter-based beams. Score spacing changes engraving width; crowded passages receive more room automatically. Accidentals follow the key signature and reset each measure. Explicit track staff assignments take priority; RH/LH track names guide display hand placement. Copies of the same quantized pitch and duration are printed once. Original MIDI playback and scoring retain every event and its original timing.

MIDI does not encode authoritative written notation. Display timing is rounded to a 32nd-note grid; spelling, voices and rhythms are inferred. Tuplets, grace notes, pickups and expressive articulation may differ from a published score. No quantization is applied to the native playback clock or the stored original.

The player uses the same paper-white score, centered marker and orange play/pause control as Staff. Restart sits on the marker; the orange strip below the score seeks through the song. Numbered measures follow the imported MIDI tempo and meter maps. Hand buttons select the scored staff and dim the other staff: explicit bass/treble track assignments take priority, while split/grand tracks use middle C as the hand boundary. At least one hand stays selected. Hand selection does not alter Backing or Hear practice part, release live keys, or stop the global metronome. It is retained across song changes and app restarts.

Choose one of ten built-in MIDI pieces (five starter arrangements and five original practice exercises), upload or drop .mid/.midi, rename, assign practice/backing tracks and staff, or download the original. Remove moves a song to recoverable trash; Restore returns it. Play/Pause/Stop/Restart/seek and speed/tempo are native-clock controls. Both practice and backing parts play by default; uncheck Hear practice part beside Play to mute the practice part. Use the toolbar menu button to hide/show the navigation sidebar. Continuous timing or Pause on miss uses fresh same-tick pitch groups, independent timing tolerance and 150 ms chord assembly. Set A/B ticks or Set here to loop; choose spacing and Off/Root/Triad/Root+5/Arp/Bossa Nova accompaniment. Rate scales pattern density; voicing spacing chooses close/open. Starter chord maps are explicit; imports infer chords at ≥70% weighted coverage and leave uncertain bars silent.

Play Along's Focus mode shows only the score player and piano keys, with a Return to Play Along button. Escape also returns. Hints, Wait mode, hand selection, tempo, loops, backing and keyboard settings stay active. A hidden keyboard is temporarily shown; returning restores its previous visibility. Focus mode changes the layout without stopping playback, live notes or the global metronome.

To repeat a passage, drag the green Start and brown End vertical lines on the score or the whole-piece overview above Play. The selected passage is shaded. Dragging a boundary pauses song playback and applies the new boundary on release; live keys and global clicks continue. Drag the score to find another passage, or use Set start here / Set end here at the current playhead. Arrow keys adjust a focused boundary by one MIDI tick (Shift by 16); boundaries cannot cross. Enable Repeat selected part for unlimited repeats. Play stays inside the passage and Restart returns to its start; Pause and Stop end playback. All tempo, speed, hand, hint, scoring, Wait/Pause on miss, backing and accompaniment settings still apply. Editing tracks retains the selection for the same piece. Clear selection disables repeat and restores the whole piece. A/B tick fields in Playback settings remain available for precise edits. Selection is saved with the last song and restored paused after a service restart. Choosing another piece resets the boundaries to that piece.

## Imports & controller limits

Hide passage controls turns repeat off and hides the passage panel, overview, A/B settings, Start/End lines and shaded selection on the score. The boundaries are retained. Showing the controls again leaves repeat off until you enable it. Hiding the controls does not stop live keys or the metronome.

When Repeat selected part reaches End, the score immediately animates back to Start over about two seconds, then playback resumes. There is no delay before movement begins. Playback and scoring pause during the return; live keys and global clicks continue. Return to Start uses the same animation from the current position, resuming only if playback was already running. Pause or Stop cancels an ongoing return. Restart remains the immediate restart control. The two-second duration is independent of song speed.

SMF types 0/1 with PPQ are supported; type 2/SMPTE are rejected. Limits: 20 MB, one million events, 256 tracks, two hours, cumulative ticks ≤2,147,483,647. Originals retain tempo/meter/key maps, channels, velocities, overlapping notes and raw controllers. Pitched tracks audition as piano; drums are disabled by default. Audible playback mappings: CC64 sustain, CC7 volume, CC11 expression, CC10 pan and CC120/123 release; other controllers and pitch bend are preserved without an audition mapping. Seek/loop restores spanning notes and controller state only on playback. Dense windows cap at 4096 notes and report truncation; accompaniment/restoration capacity errors are explicit and retain live sound/clicks.

## Your data & upgrades

Settings, originals, titles, assignments, recent songs and recoverable trash live in %LOCALAPPDATA%/SightReading (or --data-dir). Quit before backing up the complete directory, including SQLite/WAL files and originals. Upgrade by replacing the application folder while retaining that separate data directory. Running state, transient questions and held notes are not restored after service restart; a browser reload reconnects to the existing native state. The service listens only on 127.0.0.1 with strict same-origin controls.

## Tuning & troubleshooting

If Windows stops the audio stream, playback can freeze because its clock comes from native audio callbacks. The service now detects this and refreshes/reopens WASAPI automatically. Live key ownership, metronome state and song position are retained; playback resumes when the output returns. If the selected output remains unavailable, Audio / MIDI reports the error and retries each second. An explicitly chosen output is retained by name; Default follows the current Windows default.

Start with shared WASAPI at the requested 48 kHz/128 frames; Audio / MIDI reports the actual format and API output latency, which is not measured key-to-audible latency. For unavailable audio check output selection, another application's exclusive access and sample files. Device-reopen failures are shown; Quit/Start releases the device cleanly. Check disabled inputs/channels, monitoring, output mode and volume for missing notes. Capacity refuses new voices rather than stealing existing live sound: 1024 sampler layers, 960 attack slots plus 64 release reserve, separate playback/example/click ownership. Overload and underflow diagnostics are cumulative. Physical MIDI latency/hotplug, external output, wheel and audible dropout certification need actual hardware measurements; see the release verification report for measured coverage.

## Play Along finger numbers

Select a MIDI piece and choose Generate fingerings. The local ArGNN research model assigns suggested fingers separately for each hand (1 thumb through 5 little finger). Generation runs offline in a prepared portable release. MIDI records pitches and timing, not the finger you actually played; the app cannot detect your physical finger from ordinary MIDI.

Full shows every available attack suggestion; Medium shows useful phrase starts, crossings, position changes and difficult chords; Pro shows only major position changes and difficult chords. These modes change visibility, not saved assignments. A tied continuation does not repeat its attack label. The summary includes assigned, unresolved and unsupported counts, including zero.

Cancel fingering generation stops only the inference job. You can keep playing, change songs, or navigate during generation. A completed result is saved to its original song. Missing or corrupt weights give a setup error while the rest of the library remains usable.

Select a note on the score to open Manual fingering correction, then choose Finger 1–5. A merged unison selection edits all listed note identities. Use automatic restores its generated suggestion. Saved corrections constrain regeneration. Conflicting, unsupported or unresolved notes can have no suggestion. Treat numbers as practice guidance; the bounded research-model adaptation has not been statistically evaluated against published accuracy claims.
