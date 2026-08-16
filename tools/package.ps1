$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$app = Join-Path $root "app"
$output = Join-Path $root "output"
$makeromRoot = Join-Path $env:TEMP "rebank-makerom-0.19.0"
$makerom = Join-Path $makeromRoot "makerom.exe"
$archive = "$makeromRoot.zip"
$makeromUrl = "https://github.com/3DSGuy/Project_CTR/releases/download/makerom-v0.19.0/makerom-v0.19.0-win_x86_64.zip"
$makeromSha256 = "88df4455e60556374e202d507f54ee03fac7493ec9554ab853157524b6d69db0"
$drive = $app.Substring(0, 1).ToLowerInvariant()
$msysApp = "/$drive" + $app.Substring(2).Replace("\", "/")
$clientConfig = Get-Content (Join-Path $app "config\server.mk") -Raw
$buildConfig = Get-Content (Join-Path $app "include\BuildConfig.hpp") -Raw
$versionMatch = [regex]::Match($buildConfig, 'Version\s*=\s*"(\d+)\.(\d+)\.(\d+)"')
if (-not $versionMatch.Success) {
    throw "BuildConfig version must use major.minor.patch."
}
$major = [int]$versionMatch.Groups[1].Value
$minor = [int]$versionMatch.Groups[2].Value
$patch = [int]$versionMatch.Groups[3].Value
if ($major -gt 63 -or $minor -gt 63 -or $patch -gt 15) {
    throw "BuildConfig version exceeds Nintendo title-version limits."
}
$titleVersion = ($major -shl 10) -bor ($minor -shl 4) -bor $patch

Write-Host "Building with app\config\server.mk:" -ForegroundColor Cyan
$clientConfig.Trim().Split([Environment]::NewLine) | ForEach-Object { Write-Host "  $_" }
Write-Host "Nintendo title version: $titleVersion" -ForegroundColor Cyan

if (-not (Test-Path $makerom)) {
    curl.exe -L --fail --retry 3 -o $archive $makeromUrl
    if ($LASTEXITCODE -ne 0) {
        throw "makerom download failed."
    }
    $actualHash = (Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actualHash -ne $makeromSha256) {
        throw "makerom checksum mismatch."
    }
    if (Test-Path $makeromRoot) {
        Remove-Item $makeromRoot -Recurse -Force
    }
    Expand-Archive -Path $archive -DestinationPath $makeromRoot
}

& "C:\devkitPro\msys2\usr\bin\bash.exe" -lc "cd '$msysApp' && make clean && make -j2"
if ($LASTEXITCODE -ne 0) {
    throw "Nintendo 3DS build failed."
}

New-Item -ItemType Directory -Path (Join-Path $output "cia") -Force | Out-Null
Push-Location $app
try {
    & $makerom -f cia -o "..\output\cia\ReBank.cia" -rsf "packaging\rebank.rsf" -target t -elf "rebank.elf" -icon "rebank.smdh" -desc "app:7" -ver $titleVersion
    if ($LASTEXITCODE -ne 0) {
        throw "CIA packaging failed."
    }
    & $makerom -f cci -o "..\output\ReBank.3ds" -rsf "packaging\rebank.rsf" -target t -elf "rebank.elf" -icon "rebank.smdh" -desc "app:7" -ver $titleVersion
    if ($LASTEXITCODE -ne 0) {
        throw "3DS packaging failed."
    }
    Copy-Item "rebank.3dsx" "..\output\ReBank.3dsx" -Force
} finally {
    Pop-Location
}

Get-Item (Join-Path $output "cia\ReBank.cia"), (Join-Path $output "ReBank.3ds"), (Join-Path $output "ReBank.3dsx") |
    Select-Object Name, Length, @{Name = "SHA256"; Expression = { (Get-FileHash $_.FullName -Algorithm SHA256).Hash }}