$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
$out=Join-Path $root 'docs/licenses'
New-Item -ItemType Directory -Force $out | Out-Null
$paths=@(
 '.tools/deps/sfizz/LICENSE',
 '.tools/deps/sfizz/external/abseil-cpp/LICENSE',
 '.tools/deps/sfizz/external/jsl/LICENSE.md',
 '.tools/deps/sfizz/external/st_audiofile/LICENSE.md',
 '.tools/deps/sfizz/external/st_audiofile/thirdparty/dr_libs/LICENSE',
 '.tools/deps/sfizz/external/st_audiofile/thirdparty/libaiff/LICENSE',
 '.tools/deps/sfizz/external/st_audiofile/thirdparty/wavpack/COPYING',
 '.tools/deps/sfizz/external/st_audiofile/thirdparty/stb_vorbis/LICENSE',
 '.tools/deps/sfizz/external/simde/COPYING',
 '.tools/deps/sfizz/src/external/pugixml/LICENSE.md',
 '.tools/deps/sfizz/src/external/spline/LICENSE',
 '.tools/deps/sfizz/src/external/tunings/LICENSE.md',
 '.tools/deps/sfizz/src/external/hiir/license.txt',
 '.tools/deps/sfizz/src/external/kiss_fft/COPYING',
 '.tools/deps/sfizz/external/cephes/LICENSE.txt',
 '.tools/deps/sfizz/external/atomic_queue/LICENSE',
 '.tools/deps/sfizz/external/filesystem/LICENSE',
 '.tools/deps/sfizz/src/external/cpuid/LICENSE.rst',
 '.tools/deps/sfizz/src/external/cpuid/platform/LICENSE.rst',
 '.tools/deps/portaudio/LICENSE.txt', '.tools/deps/rtmidi/LICENSE',
 '.tools/deps/json/LICENSE.MIT',
 '.tools/deps/midifile/midifile-98917df5b1bf0d6e8d4c0e5fff86d6b05343e793/LICENSE.txt',
 '.tools/llvm-mingw-20261006-ucrt-x86_64/LICENSE.TXT',
 '.tools/llvm-mingw-20261006-ucrt-x86_64/x86_64-w64-mingw32/share/mingw32/COPYING.MinGW-w64-runtime.txt',
 '.tools/llvm-mingw-20261006-ucrt-x86_64/x86_64-w64-mingw32/share/mingw32/COPYING.winpthreads.txt',
 'web/node_modules/react/LICENSE','web/node_modules/react-dom/LICENSE','web/node_modules/scheduler/LICENSE','web/node_modules/vexflow/LICENSE',
 'assets/piano/LICENSE','assets/piano/ATTRIBUTION.md','assets/music/LICENSE.md','assets/ear/README.md',
 'docs/licenses/bravura-OFL.txt','docs/licenses/academico-OFL.txt','docs/licenses/httplib-MIT.txt','docs/licenses/threadpool-zlib.txt',
 'docs/licenses/faust-attribution.txt','docs/licenses/faust-rbj-exception.txt','docs/licenses/faust-library-LGPL.txt','docs/licenses/faust-STK-4.3.txt')
$notice="Sight Reading runtime third-party notices`r`nSource pins: native/dependencies.json, native/service-dependencies.json, native/toolchain.json, web/package-lock.json.`r`nStatic sampler dependencies are included conservatively; optional JACK/Qt/libsndfile/fmidi/cxxopts and build-only Catch2/npm tools are not distributed.`r`nSQLite 3.53.0 is in the public domain (https://sqlite.org/copyright.html). Windows/UCRT system DLLs are supplied by Windows, not this package.`r`n"
$manifest=@()
foreach($path in $paths){$file=Join-Path $root $path;if(!(Test-Path $file)){throw "Missing notice source $path"};$notice+="`r`n===== $path =====`r`n"+(Get-Content -Raw $file)+"`r`n";$manifest+=@{path=$path;sha256=(Get-FileHash $file).Hash}}
[IO.File]::WriteAllText((Join-Path $out 'THIRD-PARTY-NOTICES.txt'),$notice)
$manifest | ConvertTo-Json | Set-Content "$out/notice-sources.json"
