$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root 'bin\QLock.exe'
if (-not (Test-Path $exe)) { throw 'bin\QLock.exe not found - run build.cmd first' }

$info = Get-Content (Join-Path $root 'src\AssemblyInfo.cs') -Raw
$m = [regex]::Match($info, 'AssemblyVersion\("([0-9\.]+)"\)')
if (-not $m.Success) { throw 'cannot read AssemblyVersion from src\AssemblyInfo.cs' }
$version = $m.Groups[1].Value -replace '\.0$', ''

$dist = Join-Path $root 'dist'
if (Test-Path $dist) { Remove-Item $dist -Recurse -Force }
New-Item -ItemType Directory -Path $dist | Out-Null
$stage = Join-Path $dist ('QLock-' + $version)
New-Item -ItemType Directory -Path $stage | Out-Null
Copy-Item $exe $stage
Copy-Item (Join-Path $root 'README.md') $stage
Copy-Item (Join-Path $root 'LICENSE') $stage

$zip = Join-Path $dist ('QLock-' + $version + '.zip')
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal
Remove-Item $stage -Recurse -Force

$len = (Get-Item $zip).Length
Write-Host ('packed ' + $zip + ' (' + $len + ' bytes)')

$bare = Join-Path $dist 'QLock.exe'
Copy-Item $exe $bare

$sums = Join-Path $dist 'SHA256SUMS.txt'
$rows = @()
foreach ($f in @($zip, $bare)) {
  $sha = [System.Security.Cryptography.SHA256]::Create()
  $fs = [System.IO.File]::OpenRead($f)
  $hash = $sha.ComputeHash($fs)
  $fs.Dispose(); $sha.Dispose()
  $hex = -join ($hash | ForEach-Object { $_.ToString('x2') })
  $rows += ($hex + '  ' + (Split-Path $f -Leaf))
}
Set-Content -Path $sums -Value $rows -Encoding ASCII
Write-Host ('hashes -> ' + $sums)
if ($env:GITHUB_ENV) {
  Add-Content -Path $env:GITHUB_ENV -Value ('QLOCK_ZIP=' + $zip)
  Add-Content -Path $env:GITHUB_ENV -Value ('QLOCK_SUMS=' + $sums)
}
