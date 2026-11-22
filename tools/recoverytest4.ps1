param([int]$Seconds = 60, [string]$Code = "TEST1-23456", [switch]$TypeCode)
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -Namespace K -Name N -MemberDefinition @'
[DllImport("user32.dll")] public static extern void keybd_event(byte bVk, byte bScan, int dwFlags, int dwExtraInfo);
[DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
[DllImport("user32.dll")] public static extern void mouse_event(int flags, int dx, int dy, int data, int extra);
'@
function Tap([int]$vk) { [K.N]::keybd_event([byte]$vk, 0, 0, 0); Start-Sleep -Milliseconds 90; [K.N]::keybd_event([byte]$vk, 0, 2, 0) }
function Shot([string]$name) {
  $dir = Join-Path $PSScriptRoot "..\_shots"
  if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir | Out-Null }
  Add-Type -AssemblyName System.Drawing
  $bmp = New-Object System.Drawing.Bitmap 1920, 1080
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen(0, 0, 0, 0, $bmp.Size)
  $bmp.Save((Join-Path $dir $name), [System.Drawing.Imaging.ImageFormat]::Png)
  $g.Dispose(); $bmp.Dispose()
  "shot " + $name
}

Get-Process QLock -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 2
$log = Join-Path $env:LOCALAPPDATA "QLock\qlock.log"
Remove-Item $log -ErrorAction SilentlyContinue
Start-Process -FilePath (Join-Path $PSScriptRoot "..\bin\QLock.exe") -ArgumentList ("--lock --autounlock " + $Seconds)
Start-Sleep -Seconds 4

$p = Get-Process QLock -ErrorAction Stop
$root = [System.Windows.Automation.AutomationElement]::RootElement
$cond = New-Object System.Windows.Automation.PropertyCondition([System.Windows.Automation.AutomationElement]::ProcessIdProperty, $p.Id)
$win = $root.FindFirst([System.Windows.Automation.TreeScope]::Children, $cond)
$all = $win.FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition)
$target = $null
foreach ($e in $all) {
  $n = $e.Current.Name
  $r = $e.Current.BoundingRectangle
  if ($n -like "*忘记密码*" -and $r.Width -gt 0) { $target = $e }
}
if ($target -eq $null) { "link not found"; exit 2 }
$r = $target.Current.BoundingRectangle
$x = [int]($r.X + $r.Width / 2); $y = [int]($r.Y + $r.Height / 2)
"link rect=" + $r + " click at " + $x + "," + $y
[void][K.N]::SetCursorPos($x, $y); Start-Sleep -Milliseconds 300
[K.N]::mouse_event(0x0002, 0, 0, 0, 0); Start-Sleep -Milliseconds 90; [K.N]::mouse_event(0x0004, 0, 0, 0, 0)
Start-Sleep -Milliseconds 900
Shot "r1_afterclick.png"
$after = $win.FindAll([System.Windows.Automation.TreeScope]::Descendants, [System.Windows.Automation.Condition]::TrueCondition)
foreach ($e in $after) {
  $n = $e.Current.Name; $r2 = $e.Current.BoundingRectangle
  if ($n -and $r2.Width -gt 0) { "  visible '" + $n + "' rect=" + $r2 }
}
if ($TypeCode) {
  foreach ($ch in $Code.ToCharArray()) {
    if ($ch -eq '-') { Tap 0xBD } else { Tap ([int][char]([string]$ch).ToUpper()) }
    Start-Sleep -Milliseconds 140
  }
  Shot "r2_typed.png"
  Tap 0x0D
  Start-Sleep -Seconds 4
}
$fs = [System.IO.File]::Open($log, 'Open', 'Read', 'ReadWrite'); $sr = New-Object System.IO.StreamReader($fs); $t = $sr.ReadToEnd(); $sr.Close(); $fs.Close()
$t -split "\r?\n" | Where-Object { $_ -ne "" } | Select-Object -Last 10
