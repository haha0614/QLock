$cfg = Join-Path $env:APPDATA "QLock\config.json"
$bak = "$cfg.bak"
Copy-Item $cfg $bak -Force
$json = Get-Content $cfg -Raw
$json = $json -replace '"IdleLockMinutes":\d+', '"IdleLockMinutes":1'
Set-Content -Path $cfg -Value $json -Encoding UTF8 -NoNewline
$log = Join-Path $env:LOCALAPPDATA "QLock\qlock.log"
Get-Process QLock -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 800
Remove-Item $log -ErrorAction SilentlyContinue
Start-Process -FilePath (Join-Path $PSScriptRoot "..\bin\QLock.exe")
"waiting 95s idle..."
Start-Sleep -Seconds 95
& (Join-Path $PSScriptRoot "capture.ps1") (Join-Path $PSScriptRoot "..\_shots\idle1.png") | Out-Null
$t = Get-Content $log -Raw
if ($t -match "idle lock triggered") { "IDLE LOCK OK" } else { "IDLE LOCK NOT TRIGGERED (human activity?)" }
($t -split "\r?\n") | Where-Object { $_ -ne "" } | Select-Object -Last 10
Get-Process QLock -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 500
Copy-Item $bak $cfg -Force
Remove-Item $bak -Force
"config restored: " + ((Get-Content $cfg -Raw) -match '"IdleLockMinutes":0')
