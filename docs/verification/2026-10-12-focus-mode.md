# Play Along Focus mode

Focus mode hides the library, song settings, navigation, global toolbar and keyboard settings. The existing score player and piano keyboard remain mounted. A sticky Return to Play Along button or Escape restores the normal layout and previous scroll position. Previously hidden keys appear temporarily; keyboard size, range, labels and velocity remain unchanged. Native transport settings are not rewritten.

Verification:

- Production TypeScript/Vite build passed.
- All 30 frontend tests passed.
- All ten native/browser integration suites passed.
- New Focus regression verifies settings and song position preservation, native Wait-mode scoring, hints both enabled and disabled, keyboard size and visibility restoration, return button, Escape and mobile width.
- Final focused regression also verifies entering Focus mode preserves a sounding live voice and the global metronome.
- Existing Play Along tab visually checked using Ode to Joy with hints enabled; screenshot `play-along-focus-mode.jpg`.

No native engine changes or new dependencies. No physical MIDI input was connected during verification.
