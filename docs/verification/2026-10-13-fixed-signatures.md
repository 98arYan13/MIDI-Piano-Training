# Fixed key signatures and passing-note fade

The shared Staff/Play Along renderer measures the clef, key signature and meter with VexFlow instead of fixing the gutter at 118px. Longer signatures have enough space, including seven sharps/flats. A 28px gradient fades scrolling notation behind the stationary section; fixed staff lines stay continuous. The active key/meter still follows the native playback position and seek.

Regression coverage imports three- and seven-sharp/flat MIDI fixtures, checks all signature glyphs on both staves and verifies the gradient edge and glyph bounds. No native sound or transport settings change.

Verification: production build, all 33 frontend tests and all ten native/browser suites passed. Existing local Play Along tab visually checked at Bach measure 3; staff lines remain continuous through the fade. No physical MIDI hardware was used.
