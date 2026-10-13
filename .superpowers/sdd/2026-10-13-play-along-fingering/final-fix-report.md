# Integrated final source fix wave

Base: `6d18c7f13dce9bccfcb88b1110991067d172e53f`. One scoped wave addresses all three P2 findings and the README licensing-switch omission in `branch-review.md`. No agents dispatched by this worker; no model escalation.

## Changes

- Ordinary inference eligibility is inclusive MIDI 21..108. Right-hand C8 reaches the actual checkpoint. The left reflection still deliberately requires transformed 21..107; unsupported reflected windows retain fixed manual anchors. Final assignment assembly also preserves valid manual anchors when unsupported chord grouping skips decoding.
- One small shared native predicate handles unsigned JSON integers before separately accepting signed nonnegative integers. Commit and PATCH preserve UINT64_MAX without bool/float/string coercion.
- Per-hand ordered tick maps replace repeated scans of accumulated attacks. Attack ordering, maximum end tick, original note order within a chord and all category thresholds remain unchanged; grouping is O(N log N) rather than quadratic.
- All README setup/package examples show the required confirmation switch and state that applicable rights must first be established. Documentation does not assume those rights.

## Exact RED commands and observations

```powershell
.tools/fingering/python/python.exe -I tests/fingering-model.py FingeringModelTests.test_c8_and_manual_anchors_with_actual_models
```

Before implementation: failed three assertions, ordinary right/left C8 returned null and manual left C8 was discarded. The initial test expected unanchored left C8 to decode; inspection of the actual reflection showed that expectation exceeded the deliberate transformed-input contract. Corrected that expectation to `transformedPitchOutOfRange`, while retaining both-hand manual-anchor assertions and right-hand actual C8 inference. Added an oversized-chord manual-anchor assertion in the same regression.

```powershell
$env:PATH="$PWD/.tools/llvm-mingw-20261006-ucrt-x86_64/bin;$env:PATH"
$env:TEMP="$PWD/build/fingering-native/test-tmp"
$env:TMP=$env:TEMP
$cm=Get-ChildItem .tools -Recurse -Filter cmake.exe | Select-Object -First 1 -ExpandProperty FullName
& $cm --build build/fingering-native --parallel 4
& build/fingering-native/native/storage_tests.exe 'Fingering commit preserves full*'
```

Before implementation: failed `Invalid fingering assignment.` for UINT64_MAX. An earlier test construction fingerprinted mixed signed/unsigned in-memory JSON before persistence; corrected the fixture to fingerprint canonical stored JSON, matching production job creation. Large grouping regression tests exact categories/order, without a flaky elapsed-time assertion; it is preservation coverage rather than an intentionally failing behavior test.

## GREEN verification

```powershell
.tools/fingering/python/python.exe -I tests/fingering-model.py
& $cm --build build/fingering-native --parallel 4
& build/fingering-native/native/storage_tests.exe
& build/fingering-native/native/native_tests.exe '~[metronome]'
node --check tests/fingering-service.mjs
git diff --check
Get-FileHash fingering/model.py -Algorithm SHA256
```

- Final Python run: 16 actual-weight tests, 13.026 seconds, OK.
- Alternate build succeeded. Storage: 40,101 assertions / 16 cases, all passed. Native non-metronome: 291,155 assertions / 143 cases, all passed.
- Storage coverage includes exact UINT64_MAX commit/edit and negative/bool/float/string atomic rejection. Large fixture covers 20,000 sequential attacks in forward/reverse order with exact phrase/empty-category equality; retained tests cover both hands, chord categories, crossing and position thresholds.
- Syntax and whitespace checks pass. Model SHA-256: `14c1408d8d24c5c1d5d65a74477f93e7571bc51fdcac66d894cba5a026656580`.

Root-agent actual HTTP integration passed, exit 0:

```powershell
$env:PATH="$PWD/.tools/llvm-mingw-20261006-ucrt-x86_64/bin;$env:PATH"
$env:TEMP="$PWD/build/fingering-native/test-tmp"
$env:TMP=$env:TEMP
$env:SR_FINGERING_EXECUTABLE='build/fingering-native/native/sightreading.exe'
node tests/fingering-service.mjs
```

Output: `PASS real ArGNN assigned; full uint64 PATCH/invalid IDs/C8 anchor; busy/cancel/stale/trash/original song; transport/live ownership; restart`. Root used ordinary approved local TCP access outside the restricted socket sandbox. Node emitted its ordinary experimental SQLite warning.

 Worker-local TCP was denied by sandbox permissions, so two initial startup attempts timed out and cleaned their disposable directories. Root's first permitted HTTP run reached the new fixture but revealed that GET song returns metadata, not notes; corrected the assertion to read only the stopped test process's isolated SQLite model. No product API was altered.

## Self-review and limits

Read the complete review and inspected the final scoped diff. Shared ID validation short-circuits unsigned values before signed conversion. Fixtures use canonical stored fingerprints, raw uint64 HTTP JSON to avoid JS precision loss, isolated databases, and close SQLite before native restarts. The classifier retains note pointers into the unchanged song vector and map key order matches prior tick sorting. Existing meaningful tests remain.

No original executable, user's library, global settings, installer, dependency download, DLL bundle, package license grant, Graphify update or independent metronome test was performed. Physical MIDI/audio evidence and exact portable offline/clean-PATH package proof remain outside this wave. Applicable redistribution rights and original-app activation remain coordinated by the parent. Native grouping still runs under storage's mutex and allocates O(N) grouping state; this wave removes the reviewed quadratic scan, not all costs of permitted large songs.
