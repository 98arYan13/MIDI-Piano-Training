# Native piano foundation

Project-local tools: LLVM-MinGW **20261006** UCRT x86_64, CMake **4.4.4**, Ninja **1.13.2**. Obtain official portable archives only; extract beneath `.tools/`. No global IDE/settings are needed. Tool binaries, dependencies, builds and samples are ignored by Git.

Run `pwsh scripts/setup-native.ps1` then `pwsh scripts/build-native.ps1 -Tests`. Source revisions and licenses are pinned in `dependencies.json`; sfizz's recursive dependencies are pinned by its upstream Git submodules. The sample manifest pins each unchanged library file using its Git blob checksum, and setup verifies actual contents before use. Offline builds work after provisioning.

Native audio uses PortAudio's Windows WASAPI backend and RtMidi's Windows MIDI backend, with sfizz's real sample streaming/preloading. See `CONTRACT.md` for control and offline rendering interfaces. Missing/corrupt assets must leave piano readiness false and produce an actionable error; there is no oscillator fallback.

Physical MIDI connection/reconnection, loopback latency and long-session dropout claims require an actual MIDI keyboard and wired audio measurement. Automated rendering tests do not substitute for these measurements.
