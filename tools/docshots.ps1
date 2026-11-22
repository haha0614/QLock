# 重新生成 docs 里的两张截图：锁屏用默认纯色背景，设置窗单独开。
$proj = Split-Path -Parent $PSScriptRoot
$cfgPath = Join-Path $env:APPDATA "QLock\config.json"
$backup = [System.IO.File]::ReadAllBytes($cfgPath)

Get-Process QLock -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 600

Start-Process (Join-Path $proj "bin\QLock.exe") -ArgumentList "--lock --autounlock 12"
Start-Sleep -Seconds 3
& (Join-Path $PSScriptRoot "capture.ps1") (Join-Path $proj "docs\screenshot-lock.png")
Start-Sleep -Seconds 11
Get-Process QLock -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 800

$j = Get-Content $cfgPath -Raw | ConvertFrom-Json
$j.LockOnStartup = $false
[System.IO.File]::WriteAllText($cfgPath, ($j | ConvertTo-Json -Depth 6), (New-Object System.Text.UTF8Encoding($false)))
Start-Process (Join-Path $proj "bin\QLock.exe") -ArgumentList "--settings"
Start-Sleep -Seconds 4
& (Join-Path $PSScriptRoot "shotwin.ps1") -Proc QLock -Out (Join-Path $proj "docs\screenshot-settings.png")
Get-Process QLock -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 600
[System.IO.File]::WriteAllBytes($cfgPath, $backup)
"docs screenshots updated"
