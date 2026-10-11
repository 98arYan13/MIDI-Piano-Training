param([switch]$Tests)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$compiler = Join-Path $root '.tools/llvm-mingw-20261006-ucrt-x86_64/bin'
$cmake = Get-ChildItem "$root/.tools" -Recurse -Filter cmake.exe | Select-Object -First 1 -ExpandProperty FullName
$ninja = Get-ChildItem "$root/.tools" -Recurse -Filter ninja.exe | Select-Object -First 1 -ExpandProperty FullName
if (!$cmake -or !$ninja -or !(Test-Path "$compiler/clang++.exe")) { throw 'Provision the pinned portable CMake, Ninja and LLVM-MinGW first; see native/README.md.' }
$env:PATH = "$compiler;$(Split-Path $ninja);$env:PATH"
& $cmake -S $root -B "$root/build/native" -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_C_COMPILER=$compiler/clang.exe" "-DCMAKE_CXX_COMPILER=$compiler/clang++.exe" "-DCMAKE_MAKE_PROGRAM=$ninja"
if ($LASTEXITCODE) { throw 'CMake configure failed' }
& $cmake --build "$root/build/native" --parallel 4
if ($LASTEXITCODE) { throw 'Native build failed' }
if ($Tests) { & (Join-Path (Split-Path $cmake) ctest.exe) --test-dir "$root/build/native" --output-on-failure; if ($LASTEXITCODE) { throw 'Native tests failed' } }
