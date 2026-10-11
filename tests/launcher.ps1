param([string]$Release='build/release/SightReading')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
$folder=[IO.Path]::GetFullPath((Join-Path $root $Release))
$data=Join-Path ([IO.Path]::GetTempPath()) ('sr-launcher-'+[Guid]::NewGuid().ToString('N'))
$url='http://127.0.0.1:18784'
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class TrayWindowProbe {
 public delegate bool EnumProc(IntPtr window, IntPtr context);
 [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc callback, IntPtr context);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint process);
 [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr window, System.Text.StringBuilder text, int count);
 public static bool HasTray(uint pid) { bool found=false; EnumWindows((w,c)=>{uint p;GetWindowThreadProcessId(w,out p);var text=new System.Text.StringBuilder(256);GetClassName(w,text,256);if(p==pid&&text.ToString()=="SightReadingTray")found=true;return true;},IntPtr.Zero);return found; }
}
'@
$savedPath=$env:PATH
try {
 $env:PATH=Join-Path $env:SystemRoot 'System32'
 & (Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe') -NoProfile -ExecutionPolicy Bypass -File "$folder/scripts/start.ps1" -Port 18784 -DataDir $data -NoBrowser
 if($LASTEXITCODE){throw 'Portable launcher failed'}
 $processId=(Get-NetTCPConnection -LocalPort 18784 -State Listen).OwningProcess
 if(![TrayWindowProbe]::HasTray($processId)){throw 'Native notification-area window unavailable'}
 for($i=0;$i -lt 100;$i++){$state=Invoke-RestMethod "$url/api/v1/status";if($state.pianoReady -and $state.audioReady){break};Start-Sleep -Milliseconds 100}
 if(!$state.pianoReady -or !$state.audioReady){throw 'Packaged piano/audio not ready'}
 & (Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe') -NoProfile -ExecutionPolicy Bypass -File "$folder/scripts/stop.ps1" -Port 18784
 Wait-Process -Id $processId -Timeout 15
 Write-Host 'PASS Windows PowerShell portable start/stop, System32-only PATH, native tray window, actual WASAPI/piano readiness, graceful exit.'
} finally {
 $env:PATH=$savedPath
 try {Invoke-RestMethod "$url/api/v1/shutdown" -Method Post -Headers @{Origin=$url} | Out-Null}catch{}
 $resolved=[IO.Path]::GetFullPath($data)
 if(!$resolved.StartsWith([IO.Path]::GetFullPath([IO.Path]::GetTempPath()),[StringComparison]::OrdinalIgnoreCase)){throw 'Unexpected diagnostic data path'}
 if(Test-Path $resolved){Remove-Item -LiteralPath $resolved -Recurse -Force}
}
