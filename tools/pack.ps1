$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root 'bin\QLock.exe'
if (-not (Test-Path $exe)) { throw 'bin\QLock.exe not found - run build.cmd first' }

$dist = Join-Path $root 'dist'
if (Test-Path $dist) { Remove-Item $dist -Recurse -Force }
New-Item -ItemType Directory -Path $dist | Out-Null

$out = Join-Path $dist 'QLock.exe'
Copy-Item $exe $out

$sha = [System.Security.Cryptography.SHA256]::Create()
$fs = [System.IO.File]::OpenRead($out)
$hash = $sha.ComputeHash($fs)
$fs.Dispose(); $sha.Dispose()
$hex = -join ($hash | ForEach-Object { $_.ToString('x2') })

$sums = Join-Path $dist 'SHA256SUMS.txt'
Set-Content -Path $sums -Value ($hex + '  QLock.exe') -Encoding ASCII

Write-Host ('copied ' + $out)
Write-Host ('sha256 ' + $hex)
Write-Host ('written ' + $sums)
