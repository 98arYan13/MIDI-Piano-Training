# Workspace and song restoration — 2026-10-13

The native SQLite store now retains the last tab, sidebar/Focus mode, keyboard display preferences, all existing audio/metronome/exercise settings, and the last selected song's playback options and tick position. Startup restores the song without starting playback. Explicit tab links take precedence over the saved tab.

The global Settings dialog offers Reset all settings to defaults. Reset preserves the library, track assignments, selected song and current song position. Live notes are retained, disruptive audio changes use the existing deferral, and a running metronome stays running.

Playback progress is captured each second and after transport commands, queued to a bounded native background writer with its own SQLite connection, and flushed on graceful shutdown. Checkpoint writes run outside the audio callback and playback pump. Abrupt interruption can lose the latest checkpoint; storage failures are reported in the UI.

## Verification

- Production frontend build and TypeScript check passed; all 36 frontend tests passed.
- Native build passed; all four CTest targets passed.
- `node tests/run-e2e.mjs` and `node tests/session-persistence.mjs` passed all 14 integration suites.
- `tests/session-persistence.mjs` verifies complete setting restoration, last tab on a fresh root URL, UI reload and keyboard preference changes, invalid preference rejection, paused song/tick/config restoration, progress while another tab is open, native playback attacks during a locked SQLite write transaction, reset/live/metronome/library isolation, reset persistence, and deletion of the last song.
- Keyboard regression checks immediate changes across all supported sizes; preference revisions prevent older snapshots from overwriting newer edits.
- The selection regression waits until the browser displays the native seek position before comparing rewind animation transforms.
- Desktop (1440×1000) and mobile (390×844) settings screenshots were visually inspected: `session-settings-desktop.png` and `session-settings-mobile.png`.
- Independent review identified synchronous checkpoint writes on the playback pump; the bounded writer and separate connection resolved it. Follow-up review found no remaining actionable issues.

Physical MIDI hotplug, external MIDI output and hardware latency were not measured for this change. Live-note isolation was exercised through the real native service with virtual sources; native sampled piano and WASAPI were used by integration tests.
