param([switch]$Offline)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
$pins=Get-Content "$root/native/service-dependencies.json" -Raw | ConvertFrom-Json
New-Item -ItemType Directory -Force "$root/.tools/deps/httplib","$root/.tools/deps/sqlite","$root/.tools/downloads" | Out-Null
foreach($name in @('httplib','sqlite')){
 $pin=$pins.$name
 $file=if($name-eq'httplib'){"$root/.tools/deps/httplib/httplib.h"}else{"$root/.tools/downloads/sqlite-amalgamation-3530000.zip"}
 if(!(Test-Path $file)-or(Get-FileHash $file -Algorithm SHA256).Hash-ne$pin.sha256){if($Offline){throw "Missing or invalid pinned $name source"};Invoke-WebRequest $pin.url -OutFile $file -TimeoutSec 600}
 if((Get-FileHash $file -Algorithm SHA256).Hash-ne$pin.sha256){throw "$name checksum mismatch"}
 if($name-eq'sqlite'){Expand-Archive $file "$root/.tools/deps/sqlite" -Force}
}
