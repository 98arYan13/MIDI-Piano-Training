param([string]$Destination='build/release/SightReading', [string]$NativeExecutable='build/native/native/sightreading.exe', [switch]$ConfirmRedistributionRights)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
$out=[IO.Path]::GetFullPath((Join-Path $root $Destination))
$build=[IO.Path]::GetFullPath((Join-Path $root 'build'))
if(!$out.StartsWith($build+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Release destination must be inside project build/.'}
if(Test-Path $out){throw 'Choose a fresh release destination; packaging never overwrites existing releases.'}
foreach($required in @($NativeExecutable,'web/dist/index.html','assets/piano/Samples','assets/piano/protected.sfz','docs/licenses/THIRD-PARTY-NOTICES.txt')){if(!(Test-Path (Join-Path $root $required))){throw "Missing $required. Build/provision first."}}
foreach($lesson in (Get-Content "$root/assets/music/tutorials.json" -Raw | ConvertFrom-Json)) {
 $file=Join-Path "$root/assets/music" $lesson.file
 if(!(Test-Path $file)-or(Get-FileHash $file -Algorithm SHA256).Hash-ne$lesson.download.sha256){throw "Missing or invalid tutorial $($lesson.id). Run node scripts/setup-tutorials.mjs before packaging."}
}
& "$PSScriptRoot/package-fingering.ps1" -Destination $out -VerifyOnly -ConfirmRedistributionRights:$ConfirmRedistributionRights
New-Item -ItemType Directory -Force "$out/scripts","$out/web","$out/assets","$out/docs","$out/native" | Out-Null
Copy-Item (Join-Path $root $NativeExecutable) "$out/sightreading.exe"
# Confirmed non-system PE imports, including libc++'s transitive libunwind.
foreach($dll in @('libc++.dll','libunwind.dll')){Copy-Item "$root/.tools/llvm-mingw-20261006-ucrt-x86_64/bin/$dll" $out}
Copy-Item "$root/web/dist" "$out/web" -Recurse
New-Item -ItemType Directory "$out/assets/piano" | Out-Null
foreach($item in @('Samples','Data','protected.sfz','README.md','LICENSE','ATTRIBUTION.md','manifest.json')){if(Test-Path "$root/assets/piano/$item"){Copy-Item "$root/assets/piano/$item" "$out/assets/piano" -Recurse}}
Copy-Item "$root/assets/music","$root/assets/ear" "$out/assets" -Recurse
Copy-Item "$root/scripts/start.ps1","$root/scripts/stop.ps1" "$out/scripts"
Copy-Item "$root/docs/licenses" "$out/docs" -Recurse
Copy-Item "$root/docs/verification" "$out/docs" -Recurse
Copy-Item "$root/README.md" $out
Copy-Item "$root/docs/USER-GUIDE.md" "$out/docs"
Copy-Item "$root/docs/TUTORIALS.md" "$out/docs"
Copy-Item "$root/native/CONTRACT.md" "$out/native"
if(Test-Path "$root/docs/RELEASE-VERIFICATION.md"){Copy-Item "$root/docs/RELEASE-VERIFICATION.md" "$out/docs"}
'@echo off'+"`r`n"+'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\start.ps1" %*' | Set-Content "$out/Start.cmd" -Encoding ascii
'@echo off'+"`r`n"+'powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\stop.ps1" %*' | Set-Content "$out/Stop.cmd" -Encoding ascii
& "$PSScriptRoot/package-fingering.ps1" -Destination $out -ConfirmRedistributionRights:$ConfirmRedistributionRights
& "$PSScriptRoot/release-manifest.ps1" -Destination $Destination
Write-Host "Portable offline release: $out. Run Start.cmd; no Node or compiler PATH required."
