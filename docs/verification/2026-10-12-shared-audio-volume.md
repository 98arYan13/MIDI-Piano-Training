# Shared system audio and louder piano

The old native service was observed opening Speakers (Realtek USB Audio) in exclusive mode after restart. An independent shared WASAPI render process failed with `AUDCLNT_E_DEVICE_IN_USE` (0x8889000A). Output now always uses shared WASAPI; legacy constructor and saved/client exclusive settings normalize to false. The exclusive checkbox is removed. No Windows/global audio settings were changed.

All three sampled piano buses formerly multiplied each rendered sample by 0.25. They now mix at full sample gain (4x / +12.04 dB before the existing bounded limiter), preserving velocity response, channel controllers, voice ownership and metronome gain. The master slider still ranges from mute to 100%.

Verification on 2026-10-12:

- Both new native regressions failed before the fixes and passed afterward. Gain compares live/playback/example output against the real sfizz sample renderer, checks finite bounded output, and verifies master mute.
- Full CTest: all four suites passed, including 127 engine tests, three storage tests, all 88 sampled piano keys across three velocities, and transport rendering.
- TypeScript, production web build, all 30 frontend tests, service persistence/HTTP/WebSocket integration, and browser integration passed. Frontend tests used project-local `.tools/vitest-temp` after sandbox temp-file errors.
- Separate-process native tone/WASAPI loopback test passed on Realtek speakers, including a temporary database seeded with `exclusive:true`: amplitude before 0.02000143, during startup 0.01999937, during piano playback 0.02002829, after shutdown 0.01999950. The independent audio stream was not suppressed. Captured system audio was neither recorded nor saved.
- Updated service restarted on port 8765: piano/audio ready, shared mode, 48 kHz, 128 frames, API output latency 22 ms. Existing browser tab refreshed and volume set to 100%. Screenshot: `shared-audio-volume.png`.

Coverage limits: no physical MIDI keyboard was connected. This verifies native mixing and coexistence on the current Realtek endpoint, not all hardware/drivers, subjective loudness, or acoustic MIDI latency.
