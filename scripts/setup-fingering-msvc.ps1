param([switch]$Offline, [switch]$ConfirmRedistributionRights)
$ErrorActionPreference='Stop'
if(!$ConfirmRedistributionRights){throw 'App-local MSVC redistribution requires applicable Visual Studio license rights. Confirm those rights explicitly with -ConfirmRedistributionRights; no installer will run.'}
$root=Split-Path $PSScriptRoot
$pin=(Get-Content "$root/fingering/dependencies.json" -Raw | ConvertFrom-Json).msvcRuntime
$archive="$root/.tools/fingering/downloads/vc_redist.x64.exe"
if(!(Test-Path $archive)){
 if($Offline){throw 'Missing cached MSVC redistributable archive.'}
 Invoke-WebRequest $pin.url -OutFile $archive
}
if((Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pin.sha256){throw 'MSVC archive SHA-256 mismatch.'}
# Read the exact pinned Burn cabinet byte ranges; never execute its EXE.
$stage="$root/.cache/fingering/msvc-14.44.35211"
New-Item -ItemType Directory -Force "$stage/cab0","$stage/cab1","$stage/runtime" | Out-Null
$bytes=[IO.File]::ReadAllBytes($archive)
foreach($cab in $pin.cabinets){
 if([Text.Encoding]::ASCII.GetString($bytes,$cab.offset,4) -ne 'MSCF' -or [BitConverter]::ToUInt32($bytes,$cab.offset+8) -ne $cab.bytes){throw 'Pinned MSVC cabinet structure mismatch.'}
 $slice=New-Object byte[] $cab.bytes
 [Array]::Copy($bytes,$cab.offset,$slice,0,$cab.bytes)
 [IO.File]::WriteAllBytes("$stage/$($cab.name).cab",$slice)
 & "$env:SystemRoot/System32/expand.exe" "$stage/$($cab.name).cab" '-F:*' "$stage/$($cab.name)" | Out-Null
 if($LASTEXITCODE -ne 0){throw 'MSVC cabinet extraction failed.'}
}
& "$env:SystemRoot/System32/expand.exe" "$stage/cab1/a12" '-F:*' "$stage/runtime" | Out-Null
if($LASTEXITCODE -ne 0){throw 'MSVC runtime extraction failed.'}
foreach($dll in $pin.files.PSObject.Properties){
 $file="$stage/runtime/$($dll.Name)_amd64"
 if((Get-FileHash $file -Algorithm SHA256).Hash.ToLowerInvariant() -ne $dll.Value){throw "MSVC DLL SHA-256 mismatch: $($dll.Name)"}
 Copy-Item $file "$root/.tools/fingering/python/$($dll.Name)" -Force
}
if((Get-FileHash "$stage/cab0/u4" -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pin.licenseSha256){throw 'MSVC license SHA-256 mismatch.'}
Copy-Item "$stage/cab0/u4" "$root/.tools/fingering/python/MSVC-LICENSE.rtf" -Force
Write-Host 'Extracted pinned app-local MSVC runtime; no installer, registry or PATH changes.'
