# Automatic Play Along fingering verification — 2026-10-13

ArGNN is a local CPU research-model adaptation, not a claim of published evaluation accuracy. Source revision `d0d2f433c926be5e89540cbafe38bf43820ad2d2`, embedded CPython 3.11.9 and PyTorch 2.8.0+cpu are pinned in `fingering/dependencies.json`. Both actual checkpoints are 2,949,407 bytes, left SHA-256 `949033ef5f8c0a2223ec9c56ec2682be98c0ddb57cfed6c616962df01191d2cf`, right `6430b4bbf6670571c92036298c863b69288b11d982a6b0665f1b93a7cc99f441`.

## Final automated evidence

The original live executable and user's library were preserved. Tests use `build/fingering-native/native/sightreading.exe`, separate ports and disposable data directories. Process-local PATH includes the project LLVM DLL directory; TMP/TEMP use `build/fingering-native/test-tmp`.

- `.tools/fingering/python/python.exe -I tests/fingering-model.py`: 15 tests, 13.432 seconds, OK with actual weights.
- `node web/node_modules/vitest/vitest.mjs run --root web`: 9 files, 39 tests passed. A first attempt used sandbox TEMP and two suites failed with temporary SSR-file ENOENT; rerun with project-local TMP/TEMP passed.
- `node web/node_modules/typescript/bin/tsc --noEmit -p web/tsconfig.json`: exit 0.
- `node web/node_modules/vite/bin/vite.js build web`: 136 modules built. The preexisting >500 kB bundle warning remains.
- `build/fingering-native/native/native_tests.exe '~[metronome]'`: 251,145 assertions / 141 cases passed.
- `build/fingering-native/native/storage_tests.exe`: 91 assertions / 14 cases passed.
- `node tests/play-along-fingering.mjs`: real both-hand generation, Full 44 / Medium 17 / Pro 3 labels, manual/reset, cancel, song switch, unchanged native musical transport/events, focus/narrow, seek/loop, restart, zero browser errors. Retained regressions verify recovered polling clears transient alerts with unchanged revision, and explicit zero assigned/unresolved/unsupported summary text.
- `node tests/fingering-service.mjs`: real ArGNN, numeric edits, busy/cancel/stale/trash/original song, transport/live ownership and restart passed.
- `node tests/play-along.mjs`: imports, windows, library changes, transport, ownership, browser upload/mobile passed.

## Model and coverage limits

Ordinary MIDI cannot identify the player's physical finger. Suggestions can remain unresolved/unsupported and manual corrections remain authoritative. Bounded overlapping inference adapts upstream decoding; no retraining, GPU, cloud inference, training dataset or statistical accuracy evaluation was performed. Physical MIDI hotplug/input, finger detection and audible capture were not tested in this task. Existing native/virtual ownership checks cover software behavior, not physical latency or speaker-level quality. Geometry/screenshot checks cover selected fixtures and current score windows, not every possible score.

Earlier clean-runtime Task1 measurement: all nine tests then present ran in 3.141 seconds (6.264 seconds process wall time), sampled peak Windows working set 188.172 MiB. Its framed 48-note both-hand smoke emitted 53 frames, exited 0, assigned all notes, 3.604 seconds cold process wall time and 0.150 seconds subsequent warm inference. These are local sampled observations, not promised performance maxima. The final 15-test runtime above includes later correctness regressions.

## Preserved setup incident and uncertainty

Task1 inadvertently invoked a Python installer once; the log showed no package-install action ran. Exact command recorded in `.cache/fingering/python-install.log` line 46 was `python-3.11.9-amd64.exe /quiet /log "C:\Users\Amin\Documents\ChatGPT\Sight Reading\.cache\fingering\python-install.log" InstallAllUsers=0 "TargetDir=C:\Users\Amin\Documents\ChatGPT\Sight Reading\.tools\fingering\python" Include_launcher=0 PrependPath=0 Include_test=0 Include_doc=0 Include_pip=1 Include_tcltk=0 Include_dev=1`.

The log's i101 lines 95, 103, 110, 115, 131 and 155 showed existing JustForMe core/exe/dev/lib/tools/pip package states Present (including pip_JustForMe). i201 line 381 recorded pip_JustForMe Present, execute None, dependency Register; i323/i325 lines 395–406 recorded provider registration actions for `{B074012B-9B85-4049-BA01-A58A8C4C2236}`, `{C038789C-DCB5-42D3-8C51-3BC9DDB26B90}`, `{CEE03ABD-D5F9-4104-BFA5-520711B8D71F}`, `{9AFDC691-40E5-4B15-835F-9A524AC4672C}`, `{29CEC70F-9D96-472B-B6A1-AE7578376985}` and `{1D653E80-09B2-40AB-9530-7DF12E632F8A}`. i399 line 409 said Apply complete, code 0. No pre-apply provider snapshot exists, so whether these registrations were newly created or refreshed remains indeterminate. PATH components were absent/execute None. No uninstall, registry cleanup or global-setting modification was attempted. Continuation rebuilt the project-local runtime from the verified embedded ZIP and installed exact cached wheels only.

Remaining affected browser set passed: `play-along-drag`, `play-along-selection`, `session-persistence`, `final-notation`, `dense-score`, `focus-mode`, and `tutorial-library` (83 tutorials). The first drag run failed its existing mobile score-Y stability assertion immediately after viewport resize; a single focused rerun without product/test change passed. This timing sensitivity is recorded, not hidden. Session persistence emitted Node's ordinary experimental SQLite warning.

Selected inspected evidence: [both-hand Full labels](2026-10-13-fingering-full.png), [narrow controls and score](2026-10-13-fingering-narrow.png), [accidental chords and tied held notes](2026-10-13-fingering-held.png), and [digit/music ink geometry, density and transport comparison](2026-10-13-fingering-geometry.json). Task5's retained ink checks exclude intentionally crossing loop/playhead overlays and verify no >1 px digit/music collisions in the selected polyphonic window. Tied held-note coverage asserts one attack label.

## Portable release status and MSVC gate

The release source is prepared; the exact new portable package has not yet been assembled or certified. Official PE-import audit found MSVCP140.dll, VCRUNTIME140.dll and VCRUNTIME140_1.dll required by CPU Torch. The current machine's successful model import can be masked by installed system runtimes. Official version 14.44.35211.0 archive and DLL/EULA hashes are now pinned; setup uses byte-range cabinet extraction and Windows expand.exe only. No redistributable installer executes. App-local redistribution is conditional on applicable Visual Studio license rights, which are awaiting the user's factual confirmation. Neither the download nor its end-user license independently authorizes sharing combined with this app.

`pwsh -NoProfile -File scripts/setup-fingering-msvc.ps1 -Offline` rejected the missing explicit rights confirmation. `pwsh -NoProfile -File scripts/package-release.ps1 -Destination build/release/SightReading-fingering-gate -NativeExecutable build/fingering-native/native/sightreading.exe` rejected the same gate before creating the destination (`Test-Path`: False). No DLL was integrated and no portable success is claimed. The planned exact-package proof includes System32-only PATH, both-hand generation, unchanged musical transport, score labels, restart equality, and actual missing-file/corrupt-hash failure injection.

Official source: [Microsoft runtime redistribution guidance](https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files), [redistribution list](https://learn.microsoft.com/en-us/visualstudio/releases/2022/redistribution), and [runtime license](https://visualstudio.microsoft.com/license-terms/vs2022-cruntime/). The worker's bounded model source SHA-256 is `14c1408d8d24c5c1d5d65a74477f93e7571bc51fdcac66d894cba5a026656580`; protocol worker source is `71867d1c29464a218580c6ed3af15f781e93a85eb6a4505e5c7d93de15d09a30`.


## Integrated source fix verification

The final source fix wave aligns ordinary A0�C8 eligibility across Python/native boundaries. Right-hand C8 uses actual inference; deliberate left reflection limits remain and fixed manual anchors survive unsupported reflected windows. Native result/PATCH identity validation preserves unsigned 0..UINT64_MAX and separately accepts signed nonnegative integers without bool/float/string coercion. Attack grouping now uses an ordered tick map rather than repeated whole-vector scans; existing tip categories remain unchanged.

Actual-weight Python coverage now contains 16 tests; the final run passed in 13.026 seconds. Alternate-build storage coverage passed 40,101 assertions / 16 cases, including UINT64_MAX commit/edit and atomic invalid-ID rejection, plus 20,000 sequential attacks in forward/reverse input order with exact category equality. Alternate-build native non-metronome coverage passed 291,155 assertions / 143 cases. The retained real-model HTTP integration passed full uint64 raw PATCH, negative/bool/float/string rejection, C8 manual anchored regeneration, transport/live ownership and restart against an isolated database. Node emitted its ordinary experimental SQLite warning. No metronome suite, original live executable, user library, installer or DLL bundling was involved. This evidence covers local source correctness; exact portable-package proof and redistribution rights remain pending.
