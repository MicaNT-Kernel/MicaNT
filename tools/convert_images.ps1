Add-Type -AssemblyName System.Drawing

$names = @('bootscreen_splash', 'conhost_desktop')
$brainDir = "C:\Users\admin\.gemini\antigravity-cli\brain\3b3b1c2e-345e-422e-a408-68d9af4f3429"

foreach ($name in $names) {
    $bmpPath = "docs\$name.bmp"
    $pngPath = "docs\$name.png"
    $brainPath = "$brainDir\$name.png"

    if (Test-Path $bmpPath) {
        $img = [System.Drawing.Image]::FromFile((Resolve-Path $bmpPath))
        $img.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)
        $img.Save($brainPath, [System.Drawing.Imaging.ImageFormat]::Png)
        $img.Dispose()
        Write-Host "Converted $bmpPath -> $pngPath & $brainPath"
    } else {
        Write-Error "File not found: $bmpPath"
    }
}
