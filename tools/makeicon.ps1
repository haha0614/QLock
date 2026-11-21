param(
  [string]$Out = ""
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
if ([string]::IsNullOrEmpty($Out)) { $Out = Join-Path $PSScriptRoot '..\src\QLock.ico' }

function New-RoundedPath([double]$x, [double]$y, [double]$w, [double]$h, [double]$r) {
  $p = New-Object System.Drawing.Drawing2D.GraphicsPath
  $d = $r * 2
  $p.AddArc([single]$x, [single]$y, [single]$d, [single]$d, 180, 90)
  $p.AddArc([single]($x + $w - $d), [single]$y, [single]$d, [single]$d, 270, 90)
  $p.AddArc([single]($x + $w - $d), [single]($y + $h - $d), [single]$d, [single]$d, 0, 90)
  $p.AddArc([single]$x, [single]($y + $h - $d), [single]$d, [single]$d, 90, 90)
  $p.CloseFigure()
  return $p
}

$sizes = @(16, 24, 32, 48, 64, 128)
$images = New-Object System.Collections.ArrayList

foreach ($s in $sizes) {
  $bmp = [System.Drawing.Bitmap]::new([int]$s, [int]$s, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
  $g.Clear([System.Drawing.Color]::Transparent)

  $accent = [System.Drawing.Color]::FromArgb(255, 96, 150, 250)
  $white = [System.Drawing.Color]::White
  $w = [double]$s

  $pad = $w * 0.02
  $bgPath = New-RoundedPath $pad $pad ($w - 2 * $pad) ($w - 2 * $pad) ($w * 0.22)
  $bgBrush = New-Object System.Drawing.SolidBrush($accent)
  $g.FillPath($bgBrush, $bgPath)

  $pen = New-Object System.Drawing.Pen($white, [single]([Math]::Max(1.4, $w * 0.085)))
  $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
  $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
  $arcW = $w * 0.36
  $arcY = $w * 0.23
  $g.DrawArc($pen, [single](($w - $arcW) / 2), [single]$arcY, [single]$arcW, [single]($w * 0.36), 180, 180)

  $bodyX = $w * 0.23
  $bodyY = $w * 0.46
  $bodyW = $w * 0.54
  $bodyH = $w * 0.34
  $bodyPath = New-RoundedPath $bodyX $bodyY $bodyW $bodyH ($w * 0.08)
  $whiteBrush = New-Object System.Drawing.SolidBrush($white)
  $g.FillPath($whiteBrush, $bodyPath)

  $keyR = $w * 0.055
  $keyX = ($w / 2) - $keyR
  $keyY = $bodyY + $bodyH * 0.28
  $g.FillEllipse($bgBrush, [single]$keyX, [single]$keyY, [single]($keyR * 2), [single]($keyR * 2))
  $stemTop = $keyY + $keyR * 1.6
  $g.FillRectangle($bgBrush, [single]($keyX + $keyR * 0.6), [single]$stemTop, [single]($keyR * 0.8), [single]([Math]::Max(1.0, ($bodyY + $bodyH - $stemTop) * 0.75)))

  $rect = New-Object System.Drawing.Rectangle(0, 0, [int]$s, [int]$s)
  $data = $bmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $stride = $data.Stride
  $buf = New-Object byte[] ($stride * $s)
  [System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $buf, 0, $buf.Length)
  $bmp.UnlockBits($data)
  $g.Dispose(); $bmp.Dispose()

  $maskStride = [int]([Math]::Floor(($s + 31) / 32.0) * 4)
  $xorSize = $s * $s * 4
  $ms = New-Object System.IO.MemoryStream
  $bw = New-Object System.IO.BinaryWriter($ms)
  $bw.Write([uint32]40)
  $bw.Write([int32]$s)
  $bw.Write([int32]($s * 2))
  $bw.Write([uint16]1)
  $bw.Write([uint16]32)
  $bw.Write([uint32]0)
  $bw.Write([uint32]$xorSize)
  $bw.Write([int32]0); $bw.Write([int32]0)
  $bw.Write([uint32]0); $bw.Write([uint32]0)
  for ($y = $s - 1; $y -ge 0; $y--) { $bw.Write($buf, $y * $stride, $s * 4) }
  $mask = New-Object byte[] ($maskStride * $s)
  $bw.Write($mask, 0, $mask.Length)
  $bw.Flush()
  [void]$images.Add($ms.ToArray())
  $ms.Dispose(); $bw.Dispose()
  Write-Host ("drew {0}x{0}" -f $s)
}

$outMs = New-Object System.IO.MemoryStream
$ow = New-Object System.IO.BinaryWriter($outMs)
$ow.Write([uint16]0)
$ow.Write([uint16]1)
$ow.Write([uint16]$images.Count)
$offset = 6 + 16 * $images.Count
for ($i = 0; $i -lt $images.Count; $i++) {
  $s = $sizes[$i]
  $dimByte = if ($s -ge 256) { [byte]0 } else { [byte]$s }
  $ow.Write($dimByte)
  $ow.Write($dimByte)
  $ow.Write([byte]0)
  $ow.Write([byte]0)
  $ow.Write([uint16]1)
  $ow.Write([uint16]32)
  $ow.Write([uint32]$images[$i].Length)
  $ow.Write([uint32]$offset)
  $offset += $images[$i].Length
}
foreach ($img in $images) { $ow.Write($img) }
$ow.Flush()
$full = [System.IO.Path]::GetFullPath($Out)
[System.IO.File]::WriteAllBytes($full, $outMs.ToArray())
$ow.Dispose(); $outMs.Dispose()
Write-Host ("wrote " + $full + " (" + (Get-Item $full).Length + " bytes)")

$ico = New-Object System.Drawing.Icon($full)
Write-Host ("reload ok: " + $ico.Width + "x" + $ico.Height)
$ico.Dispose()
