# Polyphonic notation corrections

The marked Mary passage had a complete named piano backing chord split at middle C, placing whole-note chord tones on the melody staff. The display now keeps `Piano backing` on bass unless an explicit treble/bass track assignment overrides it. All pitches and native events are retained.

The marked Fur Elise passage has a held lower D4 beneath the upper C5/B4 melody. Voice creation order incorrectly gave the lower voice upward stems and the upper melody downward beams. Measure voices now follow pitch register: upper voice up, lower voice down. Polyphonic rests use separate vertical positions, and ties curve outward from their voice rather than toward the other voice. Single-voice notation still uses automatic stems and ties.

The subsequent Bach measure 3 report exposed the other half of middle-C splitting: the named `Practice melody` arpeggio's B3 was moved into the bass accompaniment, producing an isolated flagged note and rests. Named practice melodies now stay on treble through these low notes. Explicit bass assignment still overrides the name. A new regression failed before the change and passes afterward; all 33 frontend tests and the focused final-notation browser suite pass. The existing tab was checked at tick 4844 and left paused. Screenshot: `bach-melody-staff-corrected.jpg`.

References: [LilyPond multiple voices](https://lilypond.org/doc/v2.27/Documentation/notation/multiple-voices), [meter-based beams](https://lilypond.org/doc/v2.26/Documentation/notation/beams.html).

Verification:

- Two new model regressions failed before the fix and pass afterward; all 32 frontend tests pass.
- TypeScript/Vite production build passes.
- All ten native/browser integration suites pass after the staff and voice changes.
- Following the final tie adjustment, the dense/polyphonic browser regression passes again. It checks upper/lower beam geometry, lower tie direction, note clearance, duplicate consolidation, accidental memory, hand alignment and late seek.
- Mary measure 3 and Fur Elise measures 7–8 visually checked in the existing browser. Screenshots: `mary-backing-staff-corrected.jpg`, `fur-elise-voices-corrected.jpg`.
- The local service was stopped when inspection began. It was restarted, and the stored original Fur Elise MIDI was re-imported to the running library for exact visual verification.

This change only affects notation. Imported MIDI remains an inferred practice score rather than an authoritative published edition. No physical MIDI device was used in this verification.
