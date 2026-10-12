# Shared Staff engraving verification

Staff uses the same measure-based VexFlow renderer as Play Along. Treble and bass views draw one staff; Grand draws aligned hands. Native quarter-note exercise frames are converted to notation ticks only for display. Native spellings remain authoritative, including Cb4 in Gb major. Key signatures, bar accidental memory, rests, automatic chord stems, and collision formatting are shared. Hit/miss colors remain visible. Restart is in the toolbar so it cannot cover the target note.

Verification:

- Production TypeScript/Vite build passed.
- All 30 frontend tests passed, including new Staff timing/spelling/accidental regressions.
- All nine native/browser integration suites passed, including Staff generator families, Wait/Scroll, hand selection, pause/resume, native Cb spelling, fullscreen, mobile, persistence, live-note ownership and global metronome continuity.
- After moving Restart to the toolbar, the production build and Staff integration suite passed again.
- The existing browser tab was refreshed and a generated treble exercise visually checked; left paused for review. Screenshot: `staff-shared-engraving.jpg`.

No native engine, playback or scoring changes. No physical MIDI device was connected during this check.
