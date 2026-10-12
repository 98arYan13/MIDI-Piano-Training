# Native piano foundation

Project-local tools: LLVM-MinGW **20261006** UCRT x86_64, CMake **4.4.4**, Ninja **1.13.2**. Obtain official portable archives only; extract beneath `.tools/`. No global IDE/settings are needed. Tool binaries, dependencies, builds and samples are ignored by Git.

Run `pwsh scripts/setup-native.ps1` then `pwsh scripts/build-native.ps1 -Tests`. Source revisions and licenses are pinned in `dependencies.json`; sfizz's recursive dependencies are pinned by its upstream Git submodules. The sample manifest pins each unchanged library file using its Git blob checksum, and setup verifies actual contents before use. Offline builds work after provisioning.

Native audio uses PortAudio's Windows WASAPI backend and RtMidi's Windows MIDI backend, with sfizz's real sample streaming/preloading. See `CONTRACT.md` for control and offline rendering interfaces. Missing/corrupt assets must leave piano readiness false and produce an actionable error; there is no oscillator fallback.

Output always uses shared WASAPI. Legacy `exclusive` options/settings are accepted but normalized to false, preventing old saved settings from silencing other applications. Piano buses use full sample gain before the master slider and bounded limiter; the former 0.25 attenuation is removed without changing MIDI velocities or metronome gain.

Optional hardware regression: after building, run `node tests/audio-coexistence.mjs` with the portable LLVM-MinGW `bin` directory on `PATH`. It plays a quiet 997 Hz tone in a separate process and verifies its WASAPI loopback amplitude before, during and after piano startup/playback, including restoration of a legacy exclusive setting. It uses a temporary database, saves no captured system audio, and requires an available float32 default output. Another application holding exclusive audio access prevents this check from running.

Physical MIDI connection/reconnection, loopback latency and long-session dropout claims require an actual MIDI keyboard and wired audio measurement. Automated rendering tests do not substitute for these measurements.
