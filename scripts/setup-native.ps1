param([switch]$SkipSamples, [switch]$DownloadTools)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$pins = Get-Content "$root/native/dependencies.json" -Raw | ConvertFrom-Json
if ($DownloadTools) {
    $tools = Get-Content "$root/native/toolchain.json" -Raw | ConvertFrom-Json
    New-Item -ItemType Directory -Force "$root/.tools/downloads" | Out-Null
    foreach ($tool in $tools.PSObject.Properties) {
        $pin=$tool.Value
        $zip=Join-Path "$root/.tools/downloads" ($tool.Name+'.zip')
        if (!(Test-Path $zip) -or (Get-FileHash $zip -Algorithm SHA256).Hash -ne $pin.sha256) {
            Invoke-WebRequest $pin.url -OutFile $zip -TimeoutSec 600
        }
        if ((Get-FileHash $zip -Algorithm SHA256).Hash -ne $pin.sha256) { throw "Tool integrity failure: $($tool.Name)" }
        $dest=if($tool.Name-eq'ninja'){"$root/.tools/ninja"}else{"$root/.tools"}
        Expand-Archive -Path $zip -DestinationPath $dest -Force
    }
}
New-Item -ItemType Directory -Force "$root/.tools/deps" | Out-Null
foreach ($entry in $pins.PSObject.Properties) {
    if ($entry.Name -eq 'salamander') { continue }
    $pin = $entry.Value
    if ($pin.archive) {
        $archive=Join-Path "$root/.tools/downloads" $pin.archiveName
        New-Item -ItemType Directory -Force (Split-Path $archive) | Out-Null
        if (!(Test-Path $archive)) { Invoke-WebRequest $pin.url -OutFile $archive -TimeoutSec 300 }
        if ((Get-FileHash $archive -Algorithm SHA256).Hash -ne $pin.sha256) { throw "Dependency archive integrity failure: $($entry.Name)" }
        $dest=Join-Path "$root/.tools/deps" $entry.Name
        if (!(Test-Path "$dest/$($pin.folder)/include/MidiFile.h")) { Expand-Archive $archive -DestinationPath $dest -Force }
        continue
    }
    $dest = Join-Path "$root/.tools/deps" $entry.Name
    if (!(Test-Path "$dest/.git")) {
        & git clone --depth 1 --branch $pin.tag $pin.url $dest
        if ($LASTEXITCODE) { throw "Download failed: $($entry.Name)" }
    }
    $head = & git -C $dest rev-parse HEAD
    if ($head -ne $pin.revision) { throw "Wrong revision: $($entry.Name) ($head)" }
    if ($pin.recursive) {
        & git -C $dest submodule update --init --recursive
        if ($LASTEXITCODE) { throw 'sfizz submodule download failed' }
    }
}
if (!$SkipSamples) {
    $manifest = Get-Content "$root/assets/piano/manifest.json" -Raw | ConvertFrom-Json
    foreach ($asset in $manifest) {
        $dest = Join-Path "$root/assets/piano" $asset.path
        New-Item -ItemType Directory -Force (Split-Path $dest) | Out-Null
        $valid = $false
        if (Test-Path $dest) {
            $blob = & git hash-object --no-filters $dest
            $valid = $blob -eq $asset.gitBlobSha1
        }
        if (!$valid) {
            $encoded = ($asset.path.Split('/') | ForEach-Object { [uri]::EscapeDataString($_) }) -join '/'
            $url = "https://raw.githubusercontent.com/sfzinstruments/SalamanderGrandPiano/$($pins.salamander.revision)/$encoded"
            Invoke-WebRequest $url -OutFile $dest -TimeoutSec 120
            $blob = & git hash-object --no-filters $dest
            if ($blob -ne $asset.gitBlobSha1) { throw "Asset integrity failure: $($asset.path)" }
        }
    }
}
$patch = "$root/native/patches/sfizz-protected.patch"
if (!(Test-Path $patch)) { throw 'Protected sampler patch missing' }
if ((Get-FileHash $patch -Algorithm SHA256).Hash -ne (Get-Content "$root/native/patches/sfizz-protected.sha256").Trim()) { throw 'Protected sampler patch integrity failure' }
if (Test-Path $patch) {
    & git -C "$root/.tools/deps/sfizz" apply --unidiff-zero --reverse --check $patch 2>$null
    if ($LASTEXITCODE -ne 0) {
        & git -C "$root/.tools/deps/sfizz" apply --unidiff-zero --check $patch
        if ($LASTEXITCODE) { throw 'Pinned sfizz patch no longer applies' }
        & git -C "$root/.tools/deps/sfizz" apply --unidiff-zero $patch
        if ($LASTEXITCODE) { throw 'sfizz protection patch failed' }
    }
}
Write-Host 'Pinned dependencies and sample integrity verified.'
