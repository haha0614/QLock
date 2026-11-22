param([string]$OutDir = "E:\111\project\QLock\_shots")

Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Win32Print {
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
"@

Get-Process QLock -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 600
$p = Start-Process "E:\111\project\QLock\bin\QLock.exe" -ArgumentList "--settings" -PassThru
Start-Sleep -Seconds 3
$proc = Get-Process QLock | Select-Object -First 1
$hwnd = $proc.MainWindowHandle
if ($hwnd -eq [IntPtr]::Zero) { Write-Output "no window"; exit 1 }

$root = [System.Windows.Automation.AutomationElement]::RootElement
$cond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ProcessIdProperty, $proc.Id)
$win = $root.FindFirst([System.Windows.Automation.TreeScope]::Children, $cond)
if ($win -eq $null) { Write-Output "no uia window"; exit 1 }

$btnCond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ControlTypeProperty, [System.Windows.Automation.ControlType]::Button)
$btns = $win.FindAll([System.Windows.Automation.TreeScope]::Descendants, $btnCond)
$wr = $win.Current.BoundingRectangle
$nav = @()
foreach ($b in $btns) {
  $r = $b.Current.BoundingRectangle
  if ($r.Left -lt ($wr.Left + 200) -and $r.Height -gt 28 -and $r.Height -lt 42) { $nav += $b }
}
$nav = $nav | Sort-Object { $_.Current.BoundingRectangle.Top }
Write-Output ("nav buttons: " + $nav.Count)

function Save-Shot([IntPtr]$h, [string]$path) {
  $rect = New-Object Win32Print+RECT
  [void][Win32Print]::GetWindowRect($h, [ref]$rect)
  $w = $rect.Right - $rect.Left
  $hh = $rect.Bottom - $rect.Top
  $bmp = New-Object System.Drawing.Bitmap($w, $hh)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $hdc = $g.GetHdc()
  [void][Win32Print]::PrintWindow($h, $hdc, 2)
  $g.ReleaseHdc($hdc)
  $g.Dispose()
  $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
  $bmp.Dispose()
  Write-Output ("saved " + $path + " " + $w + "x" + $hh)
}

$names = @("unlock", "look", "start", "hard", "about")
for ($i = 0; $i -lt $nav.Count -and $i -lt 5; $i++) {
  $pat = $null
  if ($nav[$i].TryGetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern, [ref]$pat)) {
    ([System.Windows.Automation.InvokePattern]$pat).Invoke()
  }
  Start-Sleep -Milliseconds 500
  Save-Shot ([IntPtr]$hwnd) (Join-Path $OutDir ("nav-" + $names[$i] + ".png"))
}

Get-Process QLock -ErrorAction SilentlyContinue | Stop-Process -Force
Write-Output "done"