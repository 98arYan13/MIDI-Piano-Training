# Reconstruct imported MIDI notation

User requested replacement of the unreadable dense-song renderer. Keep native sound, original imports, scoring, clock and persistence unchanged. This supersedes fixed pixels-per-second notation for Play Along only.

1. Build a tested display model from native tick, meter, key and tempo metadata. Quantize display timing to a 32nd-note grid; consolidate duplicated notes; infer hand from explicit assignments or RH/LH track names; allocate overlapping rhythms to separate voices. Fill rests, decompose dotted values and tie continuations across barlines. Document MIDI inference limits.
2. Engrave complete measures with VexFlow Voices, Formatter, key/time signatures, measure-local accidental state, beams grouped by meter and ties. Choose width from formatter minimum plus readable rhythmic spacing. Keep staff exercises on their existing renderer.
3. Map native frames to the engraved onset positions for scrolling and invert that mapping for drag seeking. Cache measure SVGs, show a persistent clef/key/meter gutter, and retain bounded native event windows.
4. Verify model tests, build, real imported Fur Elise at the reported dense passage and opening, seeded dense MIDI browser regression, paused/playing seeks and both earlier collision cases. Report any native audio integration failures separately.

Primary references: W3C MusicXML 4.0 notation tutorial; LilyPond 2.26 beams and accidental rules; installed pinned VexFlow 5 API/source. MIDI lacks authoritative written voices, spelling, beam groups, pickup and articulation information, so the output is an inferred practice score.
