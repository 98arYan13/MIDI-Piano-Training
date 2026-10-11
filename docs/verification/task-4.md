# Task 4 report

Status: DONE_WITH_CONCERNS (hardware acceptance remains; implementation and automated checks complete).
Commit: 3f6d1e3 — Add global polyphonic keyboard and native-clock notation.
Base: c1b5a86c0d742f5fa60b53f04e5335b06cbaa6da.

## Scope and files

- `web/src/PianoKeyboard.tsx`, `pianoModel.ts`, `pianoModel.test.ts`: globally mounted 88-key A0–C8 piano, C4=MIDI60, independent same-pitch pointer/computer owners with module-monotonic native note instances, chords, multitouch/pointer capture/cancel, focus/visibility recovery via virtual.disconnect, virtual sustain (button/Space), velocity, labels/settings/hide, full/focused/octave controls, native-held live/playback/example colors and sustain hatching. Keyboard listeners survive navigation and dialogs; no hardware or metronome stop command.
- `web/src/ScrollingStaff.tsx`, `notationModel.ts`, `notationModel.test.ts`: VexFlow5.0.0 offline bundled Bravura/Academico fonts; joined Grand brace/clefs, continuous fixed staff lines, separate MIDI/spelling, explicit accidentals, ledger lines, duration bars spanning segment edges, grouped chords, target/hit/miss colors. Memoized 480px SVG segments only regenerate when notation/style inputs change; a bounded visible segment window avoids retaining an entire song's SVG. RAF changes track transform only; reduced-motion users receive throttled visual updates. Clock drives visuals only.
- `web/src/StaffPreview.tsx`: clearly labeled Treble/Bass/Grand notation fixture and native-clock visual scroll/freeze. No generation/scoring/example sound implemented.
- `web/src/practice.css`: responsive original keyboard/notation layout. `App.tsx` only mounts global keyboard and Staff preview, corrects admission diagnostics. `types.ts` overload corrected boolean→number and visual timeline types reexported.
- `tests/keyboard-notation.mjs`: meaningful actual C++ HTTP/WS/WASAPI + installed Edge integration; all retained. package/lockfile pin VexFlow; README documents reproducible dependency and controls/licenses.

## Frozen reusable interface for Tasks 5/6

`web/src/notationModel.ts` exports (also reexported by shared `types.ts`):

```ts
type StaffMode = 'treble' | 'bass' | 'grand';
type SpelledPitch = {
  letter: 'c'|'d'|'e'|'f'|'g'|'a'|'b';
  accidental?: 'n'|'#'|'b'|'##'|'bb';
  octave: number;
};
type TimelineNote = {
  id: string; midiNote: number; spelling: SpelledPitch;
  startFrame: number; endFrame: number;
  staff?: 'treble'|'bass';
  duration?: 'w'|'h'|'q'|'8'|'16';
  highlight?: 'target'|'hit'|'miss';
};
type ScrollingStaffProps = {
  mode: StaffMode; notes: TimelineNote[]; sampleRate: number;
  clockFrame: () => number; pixelsPerSecond?: number;
  running?: boolean; label?: string;
};
```

Frame positions and clockFrame must share an origin/sample rate; caller can supply `native.frame()-originFrame`. For Wait mode pass a constant visual clock; for Scroll/song pass native clock adapter/transport generation-relative positions. Native Tasks5/6 own ticks/tempo-map conversion and scoring; renderer never generates music or schedules sound. Keep notes reference stable between snapshot updates to preserve SVG segment memoization. Explicit staff assignment is preferred; missing assignment splits below MIDI60 to bass and MIDI60+ to treble. Duration is conventional glyph shape; frame duration is authoritative bar length. Pass new note highlights for native results. Do not remount global PianoKeyboard on route changes. No native commands/schema changes were needed.

## RED → GREEN evidence

1. `npm.cmd test -- pianoModel.test.ts` with minimal empty behavior stubs: 4 tests failed (88 expected/0 received; width1040 expected/0; chord map missing; native on/on/off expected/empty). After implementation: 4 passed.
2. `npm.cmd test -- notationModel.test.ts` with empty Grand/frame mapping stubs: 2 failed (treble+bass expected/empty; native frame48000→100px expected/0); spelling contract test already passed. After implementation included in full11-pass suite.
3. Actual browser integration initially failed at precise Staff view accessible label; added aria-label. Synthetic touch dispatch then lacked real active pointer capture; test now uses genuine Edge CDP touch input and real native state assertions, with pointercancel on captured identity. Final passes. A first harness cleanup race after failed run was corrected by installing exit listener before kill. No stale owned harness remains.
4. Visual controller/worker screenshot review found ~70px clef-to-track staff-line gap; fixed continuous structural SVG lines and note-only segments. Controller CUA reload confirmed gap resolved, joined clefs/accidentals/ledger/duration bars readable and full/focused keyboard polished.

## Final verification (after final functional changes)

- `npm.cmd run typecheck`: exit0.
- `npm.cmd test`: 4 files /11 tests passed.
- `npm.cmd run build`: exit0, Vite119 modules. Bundle972.20KB /466.70KB gzip; Vite emits non-fatal >500KB chunk warning due bundled self-hosted notation/fonts. No remote font dependency.
- Native DLL PATH + CTest: native_protection, native_piano_render, native_storage all3 passed (9.39sec); no native changes.
- Native DLL PATH + `node tests/keyboard-notation.mjs`: PASS Grand/88/C4, real same-pitch independent browser owners, computer chords/repeat identities, navigation retains notes, genuine two-touch held chord and individual pointercancel, virtual sustain/focus recovery with other owner untouched, M shortcut, full/focused/octave/hide/settings/velocity/labels,390px responsive horizontal pan, fullscreen and native-clock transform scrolling; no browser errors.
- `node tests/browser.mjs`: PASS five tabs, native controls, fullscreen dialogs, settings, reload, mobile width, browser closure, offline resources.
- `git diff --check`: exit0. Scoped commit succeeded; tracked working tree clean.

## Visual/process lifecycle and limitations

Worker inspected retained diagnostic screenshots `web/test-results/keyboard-grand.png` and `keyboard-mobile.png` (ignored outputs). Controller used CUA live preview `http://127.0.0.1:18770`, PID4020, and confirmed visuals. Owned preview stopped after review; disposable `build/keyboard-test-data` removed after resolved-path-within-build verification. Test native/Edge processes closed; meaningful tests retained. Dependencies/tools/cache/samples stay excluded from Git. No Graphify or reference assets/code used.

No physical MIDI device was available for this task. Cancellation tests assert unrelated browser source preservation at the real native boundary, complemented by native physical/live ownership tests; they do not claim actual MIDI hardware hotplug or measured key-to-audible latency. p95 latency,30-minute dropout soak and dense10-minute song performance remain release acceptance checks. At1265×713 Grand+keyboard requires page scroll; controller accepted this for Task4 and deferred compact training layout polish to Task8. Staff fixture explicitly indicates native generator/scoring arrives Task5.
