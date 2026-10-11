# Sight Reading implementation

Implement the approved spec in docs/superpowers/specs/2026-10-11-local-piano-design.md and plan in docs/superpowers/plans/2026-10-11-local-piano.md.

- Every sub-agent must use gpt-6.1-sol with low reasoning. No model escalation. Workers and reviewers must not spawn agents.
- Native C++ handles sound, MIDI devices, metronome, playback clock, imports, scoring, and persistence. Do not substitute browser WebAudio for the native engine.
- Hardware MIDI monitoring and the metronome are global. Navigation/exercise controls must not release live notes or stop the metronome.
- Preserve sounding live voices at capacity; reject new allocations instead of stealing. Isolate live, playback, examples, and click ownership.
- Do not copy the reference project's code or assets; it declares UNLICENSED. Redistributable dependencies and attributed Salamander samples are allowed.
- Never run or request graphify update unless the human explicitly requests it.
- Retain meaningful project tests; delete only disposable diagnostic files created during this session after verification.
- Keep downloads, compiler tools, caches, dependencies, and samples out of Git. Pin versions and document reproducible setup. Never log secrets or read unrelated private files.
- Do not install a system-wide IDE or modify global settings. Use project-local tools where possible.
- Run focused tests while iterating and full relevant verification before reporting completion. Report hardware coverage limitations honestly.
