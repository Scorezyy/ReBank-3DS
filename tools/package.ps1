param(
    [switch]$Dev
)

$ErrorActionPreference = "Stop"

$buildEnv = if ($Dev) { "dev" } else { "public" }
$configSuffix = if ($Dev) { ".dev" } else { "" }

$root = Split-Path -Parent $PSScriptRoot
$app = Join-Path $root "app"
$output = if ($Dev) { Join-Path $root "output\dev" } else { Join-Path $root "output" }
$makeromRoot = Join-Path $env:TEMP "rebank-makerom-0.19.0"
$makerom = Join-Path $makeromRoot "makerom.exe"
$archive = "$makeromRoot.zip"
$makeromUrl = "https://github.com/3DSGuy/Project_CTR/releases/download/makerom-v0.19.0/makerom-v0.19.0-win_x86_64.zip"
$makeromSha256 = "88df4455e60556374e202d507f54ee03fac7493ec9554ab853157524b6d69db0"
$drive = $app.Substring(0, 1).ToLowerInvariant()
$msysApp = "/$drive" + $app.Substring(2).Replace("\", "/")
$serverConfigPath = Join-Path $app "config\server$configSuffix.mk"
if (-not (Test-Path $serverConfigPath)) {
    throw "$serverConfigPath is missing. Copy config\server$configSuffix.mk.example to config\server$configSuffix.mk and fill it in."
}
$clientConfig = Get-Content $serverConfigPath -Raw
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

Write-Host "Building ($buildEnv) with app\config\server$configSuffix.mk:" -ForegroundColor Cyan
$clientConfig.Trim().Split([Environment]::NewLine) | ForEach-Object { Write-Host "  $_" }
Write-Host "Nintendo title version: $titleVersion" -ForegroundColor Cyan
if (-not $Dev) {
    Write-Host "This is a PUBLIC build - its embedded CLIENT_HMAC_SECRET is extractable by anyone who gets the binary. Only publish it if you're fine with that (rotate the secret if it ever leaks/gets abused)." -ForegroundColor Yellow
}

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

# BUILD_ENV changes which config gets baked in via preprocessor defines, which
# make's dependency tracking doesn't see - an incremental build right after
# switching dev/public could silently keep stale objects from the other mode.
# Only force a clean rebuild when the mode actually changed since last time.
$buildEnvMarker = Join-Path $app "build\.build-env"
$previousBuildEnv = if (Test-Path $buildEnvMarker) { (Get-Content $buildEnvMarker -Raw).Trim() } else { $null }
$needsClean = $previousBuildEnv -ne $buildEnv
$jobs = [Environment]::ProcessorCount
$makeCmd = if ($needsClean) { "make clean && make -j$jobs" } else { "make -j$jobs" }
& "C:\devkitPro\msys2\usr\bin\bash.exe" -lc "cd '$msysApp' && export BUILD_ENV=$buildEnv && $makeCmd"
if ($LASTEXITCODE -ne 0) {
    throw "Nintendo 3DS build failed."
}
New-Item -ItemType Directory -Path (Join-Path $app "build") -Force | Out-Null
Set-Content -Path $buildEnvMarker -Value $buildEnv -NoNewline

New-Item -ItemType Directory -Path (Join-Path $output "cia") -Force | Out-Null
Push-Location $app
try {
    & $makerom -f cia -o "$output\cia\ReBank.cia" -rsf "packaging\rebank.rsf" -target t -elf "rebank.elf" -icon "rebank.smdh" -banner "packaging\icon\banner.bnr" -desc "app:7" -ver $titleVersion
    if ($LASTEXITCODE -ne 0) {
        throw "CIA packaging failed."
    }
    & $makerom -f cci -o "$output\ReBank.3ds" -rsf "packaging\rebank.rsf" -target t -elf "rebank.elf" -icon "rebank.smdh" -banner "packaging\icon\banner.bnr" -desc "app:7" -ver $titleVersion
    if ($LASTEXITCODE -ne 0) {
        throw "3DS packaging failed."
    }
    Copy-Item "rebank.3dsx" "$output\ReBank.3dsx" -Force
} finally {
    Pop-Location
}

Get-Item (Join-Path $output "cia\ReBank.cia"), (Join-Path $output "ReBank.3ds"), (Join-Path $output "ReBank.3dsx") |
    Select-Object Name, Length, @{Name = "SHA256"; Expression = { (Get-FileHash $_.FullName -Algorithm SHA256).Hash }}
