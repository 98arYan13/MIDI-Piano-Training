param([int]$Port=8765,[string]$DataDir,[switch]$NoBrowser)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
$exe=Join-Path $root 'sightreading.exe'
if(!(Test-Path $exe)){$exe=Join-Path $root 'build/native/native/sightreading.exe'}
if(!(Test-Path $exe)-or!(Test-Path "$root/web/dist/index.html")){throw 'Build the native service and web frontend first. See README.md.'}
$runtime=Join-Path $root '.tools/llvm-mingw-20261006-ucrt-x86_64/bin'
if(Test-Path $runtime){$env:PATH="$runtime;$env:PATH"}
$argsList=@('--port',"$Port",'--web-dir',"`"$root/web/dist`"")
if($DataDir){$argsList+=@('--data-dir',"`"$DataDir`"")}
$service=Start-Process -FilePath $exe -ArgumentList $argsList -WorkingDirectory $root -WindowStyle Hidden -PassThru
try{
 $url="http://127.0.0.1:$Port"
 $ready=$false
 for($i=0;$i-lt100;$i++){if($service.HasExited){throw 'Native service exited before listening; run the executable in a terminal for diagnostics.'};try{$status=Invoke-WebRequest "$url/api/v1/status" -UseBasicParsing -TimeoutSec 1;if($status.StatusCode-eq200){$ready=$true;break}}catch{};Start-Sleep -Milliseconds 100}
 if(!$ready){throw 'Native HTTP service did not become available'}
 Write-Host "Sight Reading: $url (native process $($service.Id)). Closing the browser leaves MIDI and clicks running. Use the notification-area icon or scripts/stop.ps1 -Port $Port to quit gracefully."
 if(!$NoBrowser){Start-Process $url}
}catch{if(!$service.HasExited){Stop-Process -Id $service.Id};throw}
