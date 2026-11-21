param([string]$Pwd = "test1234")
Add-Type -Namespace K -Name N -MemberDefinition @'
[DllImport("user32.dll")] public static extern void keybd_event(byte bVk, byte bScan, int dwFlags, int dwExtraInfo);
'@
function Tap([int]$vk) { [K.N]::keybd_event([byte]$vk, 0, 0, 0); Start-Sleep -Milliseconds 90; [K.N]::keybd_event([byte]$vk, 0, 2, 0) }
function Shot([string]$name) { & (Join-Path $PSScriptRoot "capture.ps1") (Join-Path $PSScriptRoot ("..\_shots\" + $name)) | Out-Null }

$log = Join-Path $env:LOCALAPPDATA "QLock\qlock.log"
$before = (Get-Item $log).Length
Start-Process -FilePath (Join-Path $PSScriptRoot "..\bin\QLock.exe")
Start-Sleep -Seconds 3

[K.N]::keybd_event([byte]0xA2, 0, 0, 0)
[K.N]::keybd_event([byte]0xA4, 0, 0, 0)
Start-Sleep -Milliseconds 120
Tap 0x4C
[K.N]::keybd_event([byte]0xA4, 0, 2, 0)
[K.N]::keybd_event([byte]0xA2, 0, 2, 0)
Start-Sleep -Seconds 3
Shot "h1.png"

foreach ($ch in $Pwd.ToCharArray()) { Tap ([int][char]([string]$ch).ToUpper()); Start-Sleep -Milliseconds 150 }
Start-Sleep -Milliseconds 400
Tap 0x0D
Start-Sleep -Seconds 2
Shot "h2.png"
Start-Sleep -Seconds 1

$fs = [System.IO.File]::Open($log, 'Open', 'Read', 'ReadWrite')
$fs.Seek($before, 'Begin') | Out-Null
$sr = New-Object System.IO.StreamReader($fs)
$txt = $sr.ReadToEnd(); $sr.Close(); $fs.Close()
($txt -split "\r?\n" | Where-Object { $_ -ne "" }) -join [char]10
