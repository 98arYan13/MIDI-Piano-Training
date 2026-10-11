param([int]$Port=8765)
$ErrorActionPreference='Stop'
$url="http://127.0.0.1:$Port"
Invoke-RestMethod "$url/api/v1/shutdown" -Method Post -Headers @{Origin=$url} | Out-Null
Write-Host 'Native shutdown requested; MIDI, piano and clicks stop together.'
