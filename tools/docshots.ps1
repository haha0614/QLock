# 重新生成 docs 里的两张截图：锁屏用默认纯色背景，设置窗单独开。
$proj = Split-Path -Parent $PSScriptRoot
$cfgPath = Join-Path $env:APPDATA "QLock\config.json"
$backup = [System.IO.File]::ReadAllBytes($cfgPath)

# 只改这几个字段，别用 ConvertFrom-Json 来回转：PS 5.1 会把无 BOM 的 UTF-8 当 ANSI，中文全变乱码
# 截图统一拍默认外观：Fluent 主题、默认提示文字、提示文字可见、不自动锁
$text = [System.IO.File]::ReadAllText($cfgPath, [System.Text.Encoding]::UTF8)
$text = [regex]::Replace($text, '"LockOnStartup"\s*:\s*true', '"LockOnStartup":false')
$text = [regex]::Replace($text, '"ThemeId"\s*:\s*"[^"]*"', '"ThemeId":"fluent"')
$text = [regex]::Replace($text, '"Subtitle"\s*:\s*"[^"]*"', '"Subtitle":"输入密码解锁"')
$text = [regex]::Replace($text, '"ShowSubtitle"\s*:\s*(true|false)', '"ShowSubtitle":true')
[System.IO.File]::WriteAllText($cfgPath, $text, (New-Object System.Text.UTF8Encoding($false)))

Get-Process QLock -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 600

Start-Process (Join-Path $proj "bin\QLock.exe") -ArgumentList "--lock --autounlock 12"
Start-Sleep -Seconds 3
& (Join-Path $PSScriptRoot "capture.ps1") (Join-Path $proj "docs\screenshot-lock.png")
Start-Sleep -Seconds 11
Get-Process QLock -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 800

Start-Process (Join-Path $proj "bin\QLock.exe") -ArgumentList "--settings"
Start-Sleep -Seconds 4
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
# 点左侧导航第二项（外观），让设置窗截图带上预览
$p = Get-Process QLock | Select-Object -First 1
$rootEl = [System.Windows.Automation.AutomationElement]::RootElement
$pc = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ProcessIdProperty, $p.Id)
$winEl = $rootEl.FindFirst([System.Windows.Automation.TreeScope]::Children, $pc)
$bc = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ControlTypeProperty, [System.Windows.Automation.ControlType]::Button)
$navs = @()
foreach ($b in $winEl.FindAll([System.Windows.Automation.TreeScope]::Descendants, $bc)) {
    $r = $b.Current.BoundingRectangle
    if ($r.Width -gt 100 -and $r.Width -lt 200 -and ($r.Left - $winEl.Current.BoundingRectangle.Left) -lt 220) { $navs += $b }
}
$navs = @($navs | Sort-Object { $_.Current.BoundingRectangle.Top })
if ($navs.Count -ge 2) {
    $ip = $null
    if ($navs[1].TryGetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern, [ref]$ip)) { ([System.Windows.Automation.InvokePattern]$ip).Invoke() }
    Start-Sleep -Milliseconds 800
}
& (Join-Path $PSScriptRoot "shotwin.ps1") -Proc QLock -Out (Join-Path $proj "docs\screenshot-settings.png")
Get-Process QLock -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 600
[System.IO.File]::WriteAllBytes($cfgPath, $backup)
"docs screenshots updated"