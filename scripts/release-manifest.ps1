param([string]$Destination='build/release/SightReading')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
$out=[IO.Path]::GetFullPath((Join-Path $root $Destination))
$build=[IO.Path]::GetFullPath((Join-Path $root 'build'))
if(!$out.StartsWith($build+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Release must be inside project build/.'}
$files=Get-ChildItem $out -File -Recurse | Where-Object {$_.Name -ne 'release-manifest.json'}
$manifest=$files | ForEach-Object {@{path=$_.FullName.Substring($out.Length+1).Replace('\','/');bytes=$_.Length;sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower()}}
@{format=1;createdUtc=[DateTime]::UtcNow.ToString('o');sourceCommit=(git -C $root rev-parse HEAD);workingTreeDirty=[bool](git -C $root status --porcelain);files=$manifest;totalBytes=($files | Measure-Object Length -Sum).Sum} | ConvertTo-Json -Depth 5 | Set-Content "$out/release-manifest.json" -Encoding utf8
