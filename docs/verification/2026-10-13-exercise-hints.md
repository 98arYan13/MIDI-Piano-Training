# Staff and Ear Training hints verification

Added independent persisted Hint mode controls to Staff and Ear Training. Staff highlights the current note/chord using its native staff hand assignment. Ear Training highlights the interval pitches or current melody answer group. Answered/completed targets clear the hints.

Validation on 2026-10-13:

- TypeScript/Vite production build passed.
- All 34 web tests passed.
- All four native CTest targets passed.
- All 12 native/browser integration suites passed, including exercise-hints.
- Exercise-hints checks advancing targets, chords, descending intervals, melody groups, answered targets, pause and example continuity, reload persistence, independent tabs, mobile layout, live note and metronome continuity.
- Restarted the native service with user approval and reloaded the existing in-app browser. Visually verified Staff chord keys and Ear Training interval keys. Saved the Ear Training screenshot as exercise-hints-ear.png.

Hardware MIDI input was not connected during verification. Native/browser tests exercise simulated native input and audio ownership; physical-device behavior is not claimed.
