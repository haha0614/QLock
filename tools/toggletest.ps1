Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes

$cfgPath = Join-Path $env:APPDATA "QLock\config.json"
function Read-Clock {
  $t = [System.IO.File]::ReadAllText($cfgPath)
  if ($t -match '"ShowClock"\s*:\s*(true|false)') { return $matches[1] }
  return "missing"
}

Get-Process QLock -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 600
Write-Output ("before=" + (Read-Clock))

$p = Start-Process "E:\111\project\QLock\bin\QLock.exe" -ArgumentList "--settings" -PassThru
Start-Sleep -Seconds 3
$proc = Get-Process QLock | Select-Object -First 1
$root = [System.Windows.Automation.AutomationElement]::RootElement
$cond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ProcessIdProperty, $proc.Id)
$win = $root.FindFirst([System.Windows.Automation.TreeScope]::Children, $cond)

$btnCond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ControlTypeProperty, [System.Windows.Automation.ControlType]::Button)
$btns = $win.FindAll([System.Windows.Automation.TreeScope]::Descendants, $btnCond)
$wr = $win.Current.BoundingRectangle
$nav = @()
foreach ($b in $btns) {
  $r = $b.Current.BoundingRectangle
  if ($r.Left -lt ($wr.Left + 200) -and $r.Height -gt 28 -and $r.Height -lt 42) { $nav += $b }
}
$nav = $nav | Sort-Object { $_.Current.BoundingRectangle.Top }
$pat = $null
[void]$nav[1].TryGetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern, [ref]$pat)
([System.Windows.Automation.InvokePattern]$pat).Invoke()
Start-Sleep -Milliseconds 700

$cbCond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ControlTypeProperty, [System.Windows.Automation.ControlType]::CheckBox)
$cbs = $win.FindAll([System.Windows.Automation.TreeScope]::Descendants, $cbCond)
Write-Output ("checkboxes on look pane: " + $cbs.Count)
foreach ($cb in $cbs) { Write-Output ("  name=" + $cb.Current.Name + " rect=" + $cb.Current.BoundingRectangle) }

$tp = $null
[void]$cbs[0].TryGetCurrentPattern([System.Windows.Automation.TogglePattern]::Pattern, [ref]$tp)
$toggle = [System.Windows.Automation.TogglePattern]$tp
Write-Output ("state0=" + $toggle.Current.ToggleState)
$toggle.Toggle()
Start-Sleep -Milliseconds 900
Write-Output ("after toggle=" + (Read-Clock))
$toggle.Toggle()
Start-Sleep -Milliseconds 900
Write-Output ("after restore=" + (Read-Clock))

Get-Process QLock -ErrorAction SilentlyContinue | Stop-Process -Force
Write-Output "done"