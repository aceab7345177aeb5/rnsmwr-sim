# Builds the RNSMWR-SIM lab simulation (defensive training tool, NOT malware). Requires MinGW-w64 gcc.
$ErrorActionPreference = "Stop"

# Auto-locate gcc installed via winget (WinLibs).
$winlibs = Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages\BrechtSanders.WinLibs*" -Directory -ErrorAction SilentlyContinue | Select-Object -First 1
if ($winlibs) {
    $env:PATH = "$($winlibs.FullName)\mingw64\bin;$env:PATH"
}

$ml = (Get-ChildItem "mlkem" -Filter *.c).FullName

Write-Host "[1/4] Building keygen_tool (ML-KEM-1024 key encrypter)..."
& gcc -O2 -I mlkem -I . -o keygen_tool.exe keygen_tool.c @ml -ladvapi32
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "[2/4] Generating keyblob.h (encrypted key)..."
$out = & .\keygen_tool.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$out | Set-Content -Encoding ascii keyblob.h

Write-Host "[3/6] Embedding wallpaper.png (if present)..."
if (Test-Path "wallpaper.png") {
    $bytes = [System.IO.File]::ReadAllBytes((Resolve-Path "wallpaper.png"))
    $sb = New-Object System.Text.StringBuilder
    [void]$sb.AppendLine("/* Auto-generated from wallpaper.png -- do not edit. */")
    [void]$sb.AppendLine("#ifndef WALLPAPER_PNG_H")
    [void]$sb.AppendLine("#define WALLPAPER_PNG_H")
    [void]$sb.AppendLine("#include <stdint.h>")
    [void]$sb.AppendLine("static const uint8_t WALLPAPER_PNG[] = {")
    for ($i = 0; $i -lt $bytes.Length; $i += 12) {
        $j = [Math]::Min($i + 11, $bytes.Length - 1)
        $chunk = $bytes[$i..$j]
        $line = ($chunk | ForEach-Object { "0x{0:x2}" -f $_ }) -join ", "
        [void]$sb.AppendLine("    $line,")
    }
    [void]$sb.AppendLine("};")
    [void]$sb.AppendLine("#define WALLPAPER_PNG_SIZE $($bytes.Length)")
    [void]$sb.AppendLine("#endif")
    $sb.ToString() | Set-Content -Encoding ascii wallpaper_png.h
    Write-Host "embedded wallpaper.png ($($bytes.Length) bytes)"
} else {
    Set-Content -Encoding ascii wallpaper_png.h "#ifndef WALLPAPER_PNG_H`r`n#define WALLPAPER_PNG_H`r`n#include <stdint.h>`r`nstatic const uint8_t WALLPAPER_PNG[] = { 0 };`r`n#define WALLPAPER_PNG_SIZE 0`r`n#endif"
    Write-Host "no wallpaper.png found - using procedural fallback"
}

Write-Host "[4/6] Embedding alert.wav (if present)..."
if (Test-Path "alert.wav") {
    $b = [System.IO.File]::ReadAllBytes((Resolve-Path "alert.wav"))
    $sb = New-Object System.Text.StringBuilder
    [void]$sb.AppendLine("/* Auto-generated from alert.wav -- do not edit. */")
    [void]$sb.AppendLine("#ifndef SOUND_WAV_H")
    [void]$sb.AppendLine("#define SOUND_WAV_H")
    [void]$sb.AppendLine("#include <stdint.h>")
    [void]$sb.AppendLine("static const uint8_t WAV_ALERT[] = {")
    for ($i = 0; $i -lt $b.Length; $i += 12) {
        $j = [Math]::Min($i + 11, $b.Length - 1)
        $line = (($b[$i..$j]) | ForEach-Object { "0x{0:x2}" -f $_ }) -join ", "
        [void]$sb.AppendLine("    $line,")
    }
    [void]$sb.AppendLine("};")
    [void]$sb.AppendLine("#define WAV_ALERT_SIZE $($b.Length)")
    [void]$sb.AppendLine("#endif")
    $sb.ToString() | Set-Content -Encoding ascii sound_wav.h
    Write-Host "embedded alert.wav ($($b.Length) bytes)"
} else {
    Set-Content -Encoding ascii sound_wav.h "#ifndef SOUND_WAV_H`r`n#define SOUND_WAV_H`r`n#include <stdint.h>`r`nstatic const uint8_t WAV_ALERT[] = { 0 };`r`n#define WAV_ALERT_SIZE 0`r`n#endif"
    Write-Host "no alert.wav found - sound disabled"
}

Write-Host "[5/6] Extracting instructions.gif frames (if present)..."
if (Test-Path "instructions.gif") {
    Add-Type -AssemblyName System.Drawing
    $gif = [System.Drawing.Image]::FromFile((Resolve-Path "instructions.gif"))
    $dim = New-Object System.Drawing.Imaging.FrameDimension($gif.FrameDimensionsList[0])
    $frames = $gif.GetFrameCount($dim)
    $w = $gif.Width
    $h = $gif.Height
    if ($frames -lt 1 -or $w -lt 1 -or $h -lt 1) {
        Write-Host "invalid gif - disabled"
    } else {
        $sb = New-Object System.Text.StringBuilder
        [void]$sb.AppendLine("/* Auto-generated from instructions.gif -- do not edit. */")
        [void]$sb.AppendLine("#ifndef GIF_FRAMES_H")
        [void]$sb.AppendLine("#define GIF_FRAMES_H")
        [void]$sb.AppendLine("#include <stdint.h>")
        [void]$sb.AppendLine("#define GIF_FRAME_COUNT $frames")
        [void]$sb.AppendLine("#define GIF_FRAME_W $w")
        [void]$sb.AppendLine("#define GIF_FRAME_H $h")
        $delays = @()
        $ptrs = @()
        for ($i = 0; $i -lt $frames; $i++) {
            $gif.SelectActiveFrame($dim, $i)
            $bmp = New-Object System.Drawing.Bitmap($w, $h)
            $gr = [System.Drawing.Graphics]::FromImage($bmp)
            $gr.Clear([System.Drawing.Color]::Transparent)
            $gr.DrawImage($gif, 0, 0, $w, $h)
            $gr.Dispose()
            $rect = New-Object System.Drawing.Rectangle(0, 0, $w, $h)
            $data = $bmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
            $bytes = New-Object byte[] ($w * $h * 4)
            [System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $bytes.Length)
            $bmp.UnlockBits($data)
            $bmp.Dispose()
            $delay = 100
            try {
                $pi = $gif.GetPropertyItem(0x5100)
                if ($pi -and $pi.Value.Length -ge (($i + 1) * 4)) {
                    $delay = [System.BitConverter]::ToInt32($pi.Value, $i * 4)
                }
            } catch {
                $delay = 100
            }
            if ($delay -lt 5) { $delay = 100 }
            $delays += $delay
            $ptrs += "GIF_FRAME_$i"
            [void]$sb.AppendLine("static const uint8_t GIF_FRAME_$i[$($bytes.Length)] = {")
            for ($k = 0; $k -lt $bytes.Length; $k += 12) {
                $kj = [Math]::Min($k + 11, $bytes.Length - 1)
                $line = (($bytes[$k..$kj]) | ForEach-Object { "0x{0:x2}" -f $_ }) -join ", "
                [void]$sb.AppendLine("    $line,")
            }
            [void]$sb.AppendLine("};")
        }
        [void]$sb.AppendLine("static const uint8_t *GIF_FRAMES[$frames] = { $(($ptrs) -join ', ') };")
        [void]$sb.AppendLine("static const uint32_t GIF_FRAME_DELAY_MS[$frames] = { $(($delays) -join ', ') };")
        [void]$sb.AppendLine("#endif")
        $sb.ToString() | Set-Content -Encoding ascii gif_frames.h
        Write-Host "embedded instructions.gif ($frames frames, ${w}x${h})"
        $gif.Dispose()
    }
} else {
    Set-Content -Encoding ascii gif_frames.h "#ifndef GIF_FRAMES_H`r`n#define GIF_FRAMES_H`r`n#include <stdint.h>`r`n#define GIF_FRAME_COUNT 0`r`n#define GIF_FRAME_W 0`r`n#define GIF_FRAME_H 0`r`nstatic const uint8_t *GIF_FRAMES[1] = { NULL };`r`nstatic const uint32_t GIF_FRAME_DELAY_MS[1] = { 100 };`r`n#endif"
    Write-Host "no instructions.gif found - GIF disabled"
}

Write-Host "[6/6] Building rnsmwr_sim.exe and taskbar_guard.exe..."
& gcc -municode -mwindows -O2 -I mlkem -I . -o rnsmwr_sim.exe rnsmwr_sim.c @ml -ladvapi32 -lpropsys -lshell32 -lole32 -luuid -lwindowscodecs -lwinmm -lgdi32 -luser32
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& gcc -municode -mwindows -O2 -o taskbar_guard.exe taskbar_guard.c
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "OK. Run: .\rnsmwr_sim.exe"