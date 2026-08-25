# Generates a sample wallpaper.png (Windows 98 style) using System.Drawing.
Add-Type -AssemblyName System.Drawing

$w = 1920
$h = 1080
$bmp = New-Object System.Drawing.Bitmap($w, $h)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit

$rect = New-Object System.Drawing.Rectangle(0, 0, $w, $h)
$grad = New-Object System.Drawing.Drawing2D.LinearGradientBrush($rect, [System.Drawing.Color]::FromArgb(44, 84, 150), [System.Drawing.Color]::FromArgb(112, 160, 216), 90.0)
$g.FillRectangle($grad, $rect)
$grad.Dispose()

# sun glow
$glow = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(170, 255, 245, 190))
$g.FillEllipse($glow, 60, 40, 250, 250)
$glow.Dispose()

function hill($cx, $cy, $rx, $ry, $color) {
    $b = New-Object System.Drawing.SolidBrush($color)
    $r = New-Object System.Drawing.RectangleF(($cx - $rx), ($cy - $ry), (2 * $rx), (2 * $ry))
    $g.FillEllipse($b, $r)
    $b.Dispose()
}
hill 300 1120 820 540 ([System.Drawing.Color]::FromArgb(40, 96, 52))
hill 1680 1160 940 540 ([System.Drawing.Color]::FromArgb(34, 82, 46))
hill 900 1090 1120 580 ([System.Drawing.Color]::FromArgb(52, 118, 64))

function cloud($cx, $cy, $sc) {
    $b = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)
    $g.FillEllipse($b, (New-Object System.Drawing.RectangleF(($cx - 70 * $sc), ($cy - 30 * $sc), (140 * $sc), (60 * $sc))))
    $g.FillEllipse($b, (New-Object System.Drawing.RectangleF(($cx - 35 * $sc), ($cy - 48 * $sc), (80 * $sc), (68 * $sc))))
    $g.FillEllipse($b, (New-Object System.Drawing.RectangleF(($cx + 30 * $sc), ($cy - 35 * $sc), (75 * $sc), (60 * $sc))))
    $b.Dispose()
}
cloud 1220 210 3
cloud 620 380 2
cloud 1580 470 2
cloud 260 620 1

$font = New-Object System.Drawing.Font("Microsoft Sans Serif", 84, ([System.Drawing.FontStyle]::Bold -bor [System.Drawing.FontStyle]::Italic))
$shadow = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(20, 40, 80))
$white = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)
$g.DrawString("Microsoft Windows 98", $font, $shadow, 44, 32)
$g.DrawString("Microsoft Windows 98", $font, $white, 40, 28)
$shadow.Dispose()
$white.Dispose()
$font.Dispose()

$g.Dispose()
$bmp.Save("$PSScriptRoot\wallpaper.png", [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Host "wrote wallpaper.png"