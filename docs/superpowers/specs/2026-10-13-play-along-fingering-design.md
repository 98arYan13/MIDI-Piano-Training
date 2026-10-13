# Play Along automatic fingering — design for review

Date: 2026-10-13. Status: written spec approved by the user in chat on 2026-10-13. Execution method selected: subagent-driven development. The implementation plan still requires review before product implementation.

## Intended outcome and accepted decisions

For a selected MIDI song already stored in the local library, the user clicks **Generate fingering** and receives saved finger recommendations on the existing score. Accuracy takes priority over using a simpler rules-only generator. Use a pretrained research model; Python is explicitly allowed for this feature. Native C++ continues to own audio, MIDI, playback, scoring, MIDI song import, and persistence.

Keep the existing left/right score notation. Do not move notes between staves, change clefs, pitch spelling, rhythm, voice allocation, ties, beams, or track assignments. Add finger-number annotations to the existing engraving.

Full, Medium, and Pro change the number of visible tips without changing saved assignments. Optional manual finger corrections are accepted, but generation remains the primary workflow. Fingering-file import, MusicXML import for fingerings, and fingering JSON import are excluded.

## Selected research implementation and accuracy evidence

Use [PRamoneda/Automatic-Piano-Fingering](https://github.com/PRamoneda/Automatic-Piano-Fingering), targeting its ArGNN graph encoder and autoregressive decoder. The repository's [MIT license](https://github.com/PRamoneda/Automatic-Piano-Fingering/blob/main/LICENSE) names Pedro Ramoneda. Preserve attribution for adapted code and model artifacts.

The [models directory](https://github.com/PRamoneda/Automatic-Piano-Fingering/tree/main/models) lists `left_ArGNN.pth` and `right_ArGNN.pth`; the source provides beam decoding. ArGNN represents simultaneous and sequential note relationships, which makes it a suitable candidate for this polyphonic library.

Table 2 of the [2022 paper](https://eita-nakamura.github.io/articles/Ramoneda_PianoFingeringFromPartiallyAnnotatedScores_2022.pdf) reports, for ArGNNThumb-s and both hands, 66.84% general match, 72.62% highest match, 86.83% soft match, and 78.55% recombined match. These are benchmark measures with different definitions, not a guarantee for this app's songs. Do not market soft match as ordinary accuracy. Do not claim the listed checkpoints reproduce those results until their identity, preprocessing, and inference are verified.

The first implementation milestone is a local checkpoint compatibility check: pin an upstream commit, download those exact two files, record SHA-256 checksums, verify applicable artifact terms, instantiate the matching architecture, and run real CPU inference for both hands. Missing/incompatible weights are a feature setup error, not permission to silently substitute rules or random fingers. No retraining, training datasets, cloud inference, or GPU requirement is part of this design.

## Existing integration points

`native/src/Song.cpp` assigns each imported note an ID. `Storage` stores the song JSON in SQLite; `LibraryRoutes.cpp` returns windowed events through `SongWindow`. These are the persistence and data-delivery paths to extend.

`web/src/PlayAlong.tsx` selects a song and requests note windows. `web/src/scoreModel.ts` builds score tokens; `web/src/EngravedScore.tsx` renders them with VexFlow. Extend this path to carry and render note annotations while retaining the current musical grouping algorithm.

Avoid the existing general song-edit UI helper for fingering writes: it reloads the transport. Saving fingerings must refresh annotation data without reloading or stopping the playing song.

## Hand identity without notation changes

The model needs a left-hand sequence and a right-hand sequence because finger choices depend on the hand. Thumb is 1 and pinky is 5 on both hands, but their keyboard directions differ. Selecting a hand model is separate from placing a note on the score.

Derive the model's hand label from the current score grouping, including its existing explicit staff assignments, track-name recognition, and fallback behavior. A note currently grouped on the treble staff is supplied to the right-hand model; one grouped on the bass staff is supplied to the left-hand model. A mixed track uses the existing per-note grouping; do not introduce a new split or write back different staff assignments. Match the actual renderer precedence rather than assuming the native pitch fallback always matches it.

This is a modeling assumption. A hand can cross into the other staff in real music; this feature does not infer such crossings or reorganize the score. When the existing grouping is unsuitable, the recommendations may be weaker. The generation UI does not add staff-routing controls. Manual finger correction changes only the finger digit.

## User flow

Add **Generate fingering** and a **Fingering tips** selector near the selected song's score controls. The selector offers Full, Medium, and Pro, with Medium as the initial default. Persist the visibility preference through the existing native settings path.

Generation runs for the selected song's pitched piano notes across the full song, not only the visible score window. Practice-hand toggles and muting affect visibility/audibility, not the saved generation result. Exclude percussion and notes outside A0–C8 from fingering inference and count unsupported notes in the result summary.

While working, show a busy state, stage/progress, and Cancel; reject duplicate generation requests for that song. Existing tips remain visible until a successful atomic replacement. Completion reports assigned, unresolved, and unsupported note counts. Generation continues for its original song if the user selects another song; completion must never attach the result to the newly selected song. Cancel, failure, timeout, or service shutdown leaves previously saved assignments intact.

After generation, use **Regenerate fingering** for the same action. Preserve manually corrected values. Show a compact explanation that finger numbers are recommendations and that ordinary MIDI input does not identify which physical finger was used.

## Python inference boundary

The native service launches one project-local Python worker on demand using a process API with explicit executable and arguments. Start it hidden on Windows. Use framed JSON over standard input/output; reserve standard error for bounded diagnostics. No extra HTTP server or public port is needed. Only one generation job runs at a time, and completed jobs release the worker.

Requests contain a protocol version, job ID, song ID, content fingerprint, note IDs, pitch, original onset/end timing, resolved hand, and manual anchors. Responses return stage/progress and finally note-ID assignments, unresolved reasons, and the model/checkpoint identity. Python does inference and decoding only. C++ validates all output and commits it to SQLite.

Use CPU PyTorch with a bounded thread count. Keep audio callbacks, clock processing, and MIDI monitoring independent from inference. Pin a compatible local Python runtime, CPU PyTorch version, any necessary inference dependencies, upstream revision, and checkpoint checksums in reproducible setup metadata. Do not install training, visualization, or unrelated upstream dependencies. Keep runtimes, caches, downloaded dependencies, and checkpoint files in ignored project-local paths and out of Git. Setup may download artifacts; normal generation works offline after setup.

The wrapper must reproduce checkpoint preprocessing: pitch normalization, left-hand transformation where required, note ordering, and onset/next graph edges. Use the pretrained network in evaluation/inference mode. Use log-space beam scoring to avoid long-sequence numerical underflow; begin with beam width 10, matching the upstream inference entry point. Digit labels in upstream output are zero based; persist user-facing fingers as 1–5.

For long songs, use bounded overlapping windows of at most 256 notes per hand, with 64 notes of context from the preceding window. Prefer boundaries at rests and never split a simultaneous chord. Anchor already accepted overlap assignments and carry the held-note occupancy state into the next window. The limits are implementation choices for bounded memory, not claims about the paper; verify boundary consistency on real passages before release.

Decode with manual anchors fixed. Enforce distinct fingers for distinct simultaneously depressed pitches of one hand, including held notes. Pedal-sustained sound alone must not keep a finger occupied. Do not enforce broad ergonomic assumptions as absolute rules. If no valid candidate survives, mark affected notes unresolved instead of inventing assignments or changing the notes. Unsupported oversized chord groups are reported without unbounded graph allocations.

Use a 30-second inactivity timeout for worker messages and a 15-minute total job limit. The worker checks cancellation between windows. The native service can terminate its owned worker on cancel/timeout and clean up only its own temporary resources. Errors identify missing setup, incompatible checkpoints, unsupported material, inference failure, or stale song data.

## Persistence and consistency

Store an optional `fingering` object alongside each original note:

```json
{"finger": 3, "hand": "right", "source": "generated"}
```

Use `finger: null` with an unresolved reason where no assignment is valid; notes without a fingering object are still ungenerated. Manual changes use `source: "manual"` and retain the last unconstrained model suggestion in an optional `automaticFinger` field. Anchored regeneration must not replace that cached suggestion with the manually forced digit. Store display categories in a `tips` array on each fingering object. Store run-level model name, upstream revision, checkpoint hashes, generation timestamp, input fingerprint, and counts once on the song, not redundantly on every note. Do not show uncalibrated model probabilities as confidence or accuracy percentages.

Fingerprint note identities, original pitches/timing, resolved hand grouping, and manual anchors. Title edits and unrelated playback settings need not invalidate a run. Check the fingerprint again when committing; discard stale worker output when relevant data changed or the song was trashed. Merge under the storage mutex and commit in one SQLite transaction. Increment the library/annotation revision so cached event windows refresh, without a transport reload.

Fingerings remain attached to note IDs across seeking, looping, reopening, song selection, and application restart. Original MIDI bytes remain intact. Existing songs need no regeneration merely to remain usable, and missing fields are backwards compatible.

## Score annotations and tip density

Render fingers 1–5 above right-hand notes and below left-hand notes with readable spacing. Add annotations without changing note identities, pitches, durations, voices, or staff assignment. Reserve enough label space to avoid collisions with noteheads, ledger lines, accidentals, beams, ties, bar numbers, and chord labels. Preserve existing scrolling, dragging, seeking, and passage loops.

Keep finger data on each score pitch together with its original note ID: a chord's pitch sorting must also sort its corresponding finger values. When the existing renderer merges duplicate unisons, retain the contributing IDs; show one shared digit when they agree, or leave the merged glyph unresolved when they conflict. Editing a merged glyph applies the same manual digit to all contributing IDs.

Show the digit only on the initial attack of a tied note, without repeating it on continuation segments. Label visibility is decided using whole-song context, not the current browser window. Compute tip categories once from the accepted sequence and persist them with the generation metadata/notes so seeking does not change which tips are visible.

- **Full:** every assigned note attack.
- **Medium:** Full's phrase-start, thumb-crossing, position-change, and difficult-chord categories.
- **Pro:** Medium's thumb-crossing and major-position-change categories only.

For the initial density classifier, define a phrase start as the first event of a hand or an event after a gap of at least one quarter-note beat with no depressed notes in that hand. A thumb crossing is a non-repeated-pitch sequential transition between thumb and another digit against the hand's normal pitch/finger order. Estimate position change from the change in each attack's median hand-origin estimate, derived from white-key position and digit index with mirrored left-hand direction. A change of at least two white-key positions is a position change; at least four is major. A difficult chord is an attack with at least four distinct pitches or a span of at least an octave. These rules choose display density only; they do not overwrite model assignments or claim biomechanical certainty. Recompute affected categories after a manual correction.

Optional manual editing uses an explicit **Edit fingering** toggle. In that mode, activate a note or its digit to choose 1–5 or **Use automatic**. In ordinary practice mode, existing score dragging remains the default. Provide keyboard-accessible controls and a note description for screen readers. **Use automatic** removes that manual lock and restores the latest automatic suggestion, or leaves the note awaiting regeneration if none is available. Manual digits remain visible only where the selected density mode would show them, and regeneration treats them as fixed anchors.

## Validation and completion criteria

Before product integration, prove real CPU inference with the pinned right/left checkpoints and verify input/output conventions against upstream code. Verify deterministic results for a fixed checkpoint and input. Record measured runtime and memory on this Windows machine; published inference timings are not local measurements.

Retain project tests and add focused coverage for note-ID association, digit conversion, chords/held notes, manual anchors, window boundaries, malformed worker output, atomic save, cancellation, stale jobs, missing setup, and restart persistence. Frontend tests cover pitch/finger ordering, merged unisons, tied continuations, and density modes.

Browser verification must show generated numbers on the actual Play Along score in Full, Medium, and Pro, including chords, long notes, accidentals, loops, seeking, focus mode, and a narrow viewport. Compare musical score tokens before/after generation to prove only annotations changed. Verify that generating and editing preserve transport position/state and that no command stops global monitoring or the metronome. Do not broaden verification into unrelated metronome testing.

For quality review, use independently licensed or project-authored passages covering scales, arpeggios, repeated notes, chord changes, held inner voices, and both hands. Report assignment coverage and objectively detectable conflicts separately from agreement with editorial fingerings. Do not download or redistribute research datasets just because code is MIT licensed. If an authorized benchmark is available, compare with its documented metrics; otherwise report that musical accuracy has not been independently quantified. Reject release if real model inference, readable score rendering, restart persistence, or notation preservation remains unverified.

## Review handoff

This document records the user's accepted scope and the proposed integration details. The written spec requires user review before writing the implementation plan. After spec approval, prepare the plan and let the user review it and select the execution method before changing product code or installing inference dependencies.
