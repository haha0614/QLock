# QLock 应用图标：QTools 品牌兔子（Lucide rabbit, ISC）+ 透明底 + 品牌蓝线条
# 用法：powershell -File tools\makeicon.ps1 [-Out src\QLock.ico] [-PreviewDir _shots\icon] [-Stroke 409EFF]
param(
  [string]$Out = 'src\QLock.ico',
  [string]$PreviewDir = '',
  [string]$Stroke = '409EFF',
  [double]$SmallStroke = 2.4
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName PresentationCore
Add-Type -AssemblyName WindowsBase

$strokeColor = [System.Windows.Media.Color]::FromRgb(
  [Convert]::ToByte($Stroke.Substring(0, 2), 16),
  [Convert]::ToByte($Stroke.Substring(2, 2), 16),
  [Convert]::ToByte($Stroke.Substring(4, 2), 16))
$strokeBrush = [System.Windows.Media.SolidColorBrush]::new($strokeColor)
$strokeBrush.Freeze()

# Lucide v3 icons/rabbit.svg 原样路径（顺序、数值未改）
$rabbit = @(
  'M13 16a3 3 0 0 1 2.24 5',
  'M18 12h.01',
  'M18 21h-8a4 4 0 0 1-4-4 7 7 0 0 1 7-7h.2L9.6 6.4a1 1 0 1 1 2.8-2.8L15.8 7h.2c3.3 0 6 2.7 6 6v1a2 2 0 0 1-2 2h-1a3 3 0 0 0-3 3',
  'M20 8.54V4a2 2 0 1 0-4 0v3',
  'M7.612 12.524a3 3 0 1 0-1.6 4.3'
)

function New-Pen([double]$w) {
  $p = [System.Windows.Media.Pen]::new($strokeBrush, $w)
  $p.StartLineCap = [System.Windows.Media.PenLineCap]::Round
  $p.EndLineCap = [System.Windows.Media.PenLineCap]::Round
  $p.LineJoin = [System.Windows.Media.PenLineJoin]::Round
  return $p
}

function New-IconVisual([double]$scale, [double]$stroke, [int]$size) {
  $pen = New-Pen $stroke
  # Geometry.Parse 返回冻结几何，Transform 必须由 GeometryGroup 承载
  $tg = [System.Windows.Media.TransformGroup]::new()
  [void]$tg.Children.Add([System.Windows.Media.ScaleTransform]::new(0.8333, 0.8333))
  [void]$tg.Children.Add([System.Windows.Media.TranslateTransform]::new(2, 2))
  $dv = [System.Windows.Media.DrawingVisual]::new()
  $dc = $dv.RenderOpen()
  $dc.PushTransform([System.Windows.Media.ScaleTransform]::new($scale, $scale))
  $paths = if ($size -le 24) { @($rabbit[0], $rabbit[2], $rabbit[3], $rabbit[4]) } else { $rabbit }   # 小尺寸丢掉眼点，否则糊成一团
  foreach ($d in $paths) {
    $gg = [System.Windows.Media.GeometryGroup]::new()
    $gg.Transform = $tg
    [void]$gg.Children.Add([System.Windows.Media.Geometry]::Parse($d))
    $dc.DrawGeometry($null, $pen, $gg)
  }
  $dc.Pop()
  $dc.Close()
  return $dv
}

# 小尺寸加粗线条，否则 16px 下线条只有 1.3px、几乎看不见
function Stroke-For([int]$size) {
  if ($size -le 16) { return $SmallStroke }
  if ($size -le 24) { return [Math]::Min($SmallStroke, 2.8) }
  return 2.4
}

function Render-Bgra([int]$size) {
  $dv = New-IconVisual ($size / 24.0) (Stroke-For $size) $size
  $rtb = [System.Windows.Media.Imaging.RenderTargetBitmap]::new($size, $size, 96, 96, [System.Windows.Media.PixelFormats]::Pbgra32)
  $rtb.Render($dv)
  $stride = $size * 4
  $pix = [byte[]]::new($stride * $size)
  $rtb.CopyPixels($pix, $stride, 0)
  # Pbgra32 -> 直通 BGRA（ICO 里的 32bpp 位图不预乘）
  for ($i = 0; $i -lt $pix.Length; $i += 4) {
    $a = $pix[$i + 3]
    if ($a -eq 0 -or $a -eq 255) { continue }
    $inv = 255.0 / $a
    $pix[$i] = [byte][Math]::Min(255, [Math]::Round($pix[$i] * $inv))
    $pix[$i + 1] = [byte][Math]::Min(255, [Math]::Round($pix[$i + 1] * $inv))
    $pix[$i + 2] = [byte][Math]::Min(255, [Math]::Round($pix[$i + 2] * $inv))
  }
  return ,$pix
}

function Save-Png([string]$path, [byte[]]$bgra, [int]$w, [int]$h, [int]$zoom) {
  $src = [System.Windows.Media.Imaging.BitmapSource]::Create($w, $h, 96, 96, [System.Windows.Media.PixelFormats]::Bgra32, $null, $bgra, $w * 4)
  $img = $src
  if ($zoom -gt 1) {
    $img = [System.Windows.Media.Imaging.TransformedBitmap]::new($src, [System.Windows.Media.ScaleTransform]::new($zoom, $zoom))
  }
  $enc = [System.Windows.Media.Imaging.PngBitmapEncoder]::new()
  [void]$enc.Frames.Add([System.Windows.Media.Imaging.BitmapFrame]::Create($img))
  $fs = [System.IO.File]::Create($path)
  try { $enc.Save($fs) } finally { $fs.Dispose() }
}

if ($PreviewDir -ne '') {
  if (-not (Test-Path $PreviewDir)) { New-Item -ItemType Directory -Path $PreviewDir | Out-Null }
  Save-Png (Join-Path $PreviewDir 'qlock-128.png') (Render-Bgra 128) 128 128 1
  Save-Png (Join-Path $PreviewDir 'qlock-32x3.png') (Render-Bgra 32) 32 32 3
  Save-Png (Join-Path $PreviewDir 'qlock-16x6.png') (Render-Bgra 16) 16 16 6
  Write-Host 'previews written'
}

if ($Out -ne '') {
  $sizes = @(16, 24, 32, 48, 64, 128)
  $blobs = New-Object System.Collections.ArrayList
  foreach ($s in $sizes) {
    $pix = Render-Bgra $s
    $maskStride = [int]([Math]::Floor(($s + 31) / 32.0) * 4)
    $xorSize = $s * $s * 4
    $ms = New-Object System.IO.MemoryStream
    $bw = New-Object System.IO.BinaryWriter($ms)
    $bw.Write([uint32]40)          # biSize
    $bw.Write([int32]$s)           # biWidth
    $bw.Write([int32]($s * 2))     # biHeight = XOR + AND
    $bw.Write([uint16]1)           # biPlanes
    $bw.Write([uint16]32)          # biBitCount
    $bw.Write([uint32]0)           # biCompression = BI_RGB
    $bw.Write([uint32]$xorSize)    # biSizeImage
    $bw.Write([int32]0); $bw.Write([int32]0)
    $bw.Write([uint32]0); $bw.Write([uint32]0)
    $stride = $s * 4
    for ($y = $s - 1; $y -ge 0; $y--) { $bw.Write($pix, $y * $stride, $s * 4) }   # 自底向上
    $mask = [byte[]]::new($maskStride * $s)                                       # 全 0 = 不透明，靠 alpha 通道
    $bw.Write($mask, 0, $mask.Length)
    $bw.Flush()
    [void]$blobs.Add($ms.ToArray())
    $ms.Dispose(); $bw.Dispose()
  }
  $outMs = New-Object System.IO.MemoryStream
  $ow = New-Object System.IO.BinaryWriter($outMs)
  $ow.Write([uint16]0); $ow.Write([uint16]1); $ow.Write([uint16]$blobs.Count)
  $offset = 6 + 16 * $blobs.Count
  for ($i = 0; $i -lt $sizes.Count; $i++) {
    $dim = if ($sizes[$i] -ge 256) { [byte]0 } else { [byte]$sizes[$i] }
    $ow.Write($dim); $ow.Write($dim); $ow.Write([byte]0); $ow.Write([byte]0)
    $ow.Write([uint16]1); $ow.Write([uint16]32)
    $ow.Write([uint32]$blobs[$i].Length)
    $ow.Write([uint32]$offset)
    $offset += $blobs[$i].Length
  }
  foreach ($b in $blobs) { $ow.Write($b) }
  $ow.Flush()
  $full = [System.IO.Path]::GetFullPath($Out)
  [System.IO.File]::WriteAllBytes($full, $outMs.ToArray())
  $ow.Dispose(); $outMs.Dispose()
  Write-Host ('wrote ' + $full + ' (' + (Get-Item $full).Length + ' bytes)')
}
