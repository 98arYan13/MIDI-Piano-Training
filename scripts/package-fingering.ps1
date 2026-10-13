param([Parameter(Mandatory)][string]$Destination, [switch]$ConfirmRedistributionRights, [switch]$VerifyOnly)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
$Destination=[IO.Path]::GetFullPath($Destination)
$build=[IO.Path]::GetFullPath((Join-Path $root 'build'))
if(!$Destination.StartsWith($build+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Fingering release destination must be inside project build/.'}
$pins=Get-Content "$root/fingering/dependencies.json" -Raw | ConvertFrom-Json
$cache="$root/.tools/fingering/downloads"
function Verify($file,$hash) {
 if(!(Test-Path -LiteralPath $file) -or (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant() -ne $hash){throw "Missing or SHA-256 mismatch: $file"}
}
Verify "$cache/python-3.11.9-embed-amd64.zip" $pins.runtime.sha256
foreach($wheel in $pins.wheels.PSObject.Properties){if($wheel.Name -ne 'pip'){Verify "$cache/$($wheel.Value.filename)" $wheel.Value.sha256}}
foreach($hand in @('left','right')){Verify "$root/.tools/fingering/models/$($pins.checkpoints.$hand.file)" $pins.checkpoints.$hand.sha256}
if($ConfirmRedistributionRights){
 foreach($dll in $pins.msvcRuntime.files.PSObject.Properties){Verify "$root/.tools/fingering/python/$($dll.Name)" $dll.Value}
 Verify "$root/.tools/fingering/python/MSVC-LICENSE.rtf" $pins.msvcRuntime.licenseSha256
}
if($VerifyOnly){return}
New-Item -ItemType Directory -Force "$Destination/fingering" | Out-Null
Get-ChildItem "$root/fingering" -File | Copy-Item -Destination "$Destination/fingering"
$runtime="$Destination/.tools/fingering"
$python="$runtime/python"
New-Item -ItemType Directory -Force "$runtime/models","$python/Lib/site-packages" | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::ExtractToDirectory("$cache/python-3.11.9-embed-amd64.zip",$python)
if(!$ConfirmRedistributionRights){
 foreach($dll in $pins.msvcRuntime.files.PSObject.Properties){$file=Join-Path $python $dll.Name;if(Test-Path $file){Remove-Item -LiteralPath $file -Force}}
}
Set-Content "$python/python311._pth" @('python311.zip','.','Lib\site-packages','import site') -Encoding ascii
foreach($wheel in $pins.wheels.PSObject.Properties){if($wheel.Name -ne 'pip'){
 $archive=[IO.Compression.ZipFile]::OpenRead("$cache/$($wheel.Value.filename)")
 try{foreach($entry in $archive.Entries){
  # Extension compiler headers/static libraries are unnecessary for CPU inference.
  if($entry.FullName.EndsWith('/') -or $entry.FullName.StartsWith('torch/include/') -or $entry.FullName.EndsWith('.lib')){continue}
  $target=Join-Path "$python/Lib/site-packages" $entry.FullName
  New-Item -ItemType Directory -Force (Split-Path $target) | Out-Null
  [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$target,$true)
 }}finally{$archive.Dispose()}
}}
foreach($hand in @('left','right')){Copy-Item "$root/.tools/fingering/models/$($pins.checkpoints.$hand.file)" "$runtime/models"}
# setup-fingering supplies the archive-extracted, hash-pinned vendor runtime.
if($ConfirmRedistributionRights){
 foreach($dll in $pins.msvcRuntime.files.PSObject.Properties){Copy-Item "$root/.tools/fingering/python/$($dll.Name)" $python -Force}
 Copy-Item "$root/.tools/fingering/python/MSVC-LICENSE.rtf" $python
}
@{format=1;msvcMode=$(if($ConfirmRedistributionRights){'bundled'}else{'windows-prerequisite'});requiredDlls=@($pins.msvcRuntime.files.PSObject.Properties.Name);prerequisite='Existing Microsoft Visual C++ v14 x64 runtime on Windows; not a self-contained clean-Windows package.'} | ConvertTo-Json | Set-Content "$runtime/runtime-mode.json" -Encoding utf8
Write-Host 'Packaged verified embedded Python, CPU inference wheels and both ArGNN checkpoints; no downloads, pip or training files.'
