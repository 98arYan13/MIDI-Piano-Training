param([Parameter(Mandatory=$true)][int]$NativePid,[int]$Seconds=60,[string]$Report='build/process-load.json')
$ErrorActionPreference='Stop'
$cores=[Environment]::ProcessorCount
$before=Get-Process -Id $NativePid
$cpu=$before.CPU
$timer=[Diagnostics.Stopwatch]::StartNew()
$readings=@()
for($i=0;$i -lt $Seconds;$i++){
 Start-Sleep -Seconds 1
 $process=Get-Process -Id $NativePid
 $readings+=@{elapsedMs=$timer.ElapsedMilliseconds;cpuSeconds=$process.CPU;workingSetBytes=$process.WorkingSet64;privateBytes=$process.PrivateMemorySize64}
}
$delta=$process.CPU-$cpu
@{pid=$NativePid;logicalProcessors=$cores;elapsedMs=$timer.ElapsedMilliseconds;nativeCpuSeconds=$delta;oneCorePercent=100*$delta/$timer.Elapsed.TotalSeconds;machineCpuPercent=100*$delta/$timer.Elapsed.TotalSeconds/$cores;readings=$readings} | ConvertTo-Json -Depth 5 | Set-Content $Report
Get-Content $Report -Head 12
