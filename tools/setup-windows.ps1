$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$devkitBash = "C:\devkitPro\msys2\usr\bin\bash.exe"
$requiredCommands = "node", "npm", "docker"

foreach ($command in $requiredCommands) {
    if (-not (Get-Command $command -ErrorAction SilentlyContinue)) {
        throw "$command is required and was not found in PATH."
    }
}
if (-not (Test-Path $devkitBash)) {
    throw "devkitPro was not found at C:\devkitPro. Install the 3ds-dev toolchain first."
}

Set-Location (Join-Path $root "Server")
npm install
if ($LASTEXITCODE -ne 0) {
    throw "Server dependency installation failed."
}

& $devkitBash -lc "pacman -S --needed --noconfirm 3ds-dev 3ds-libvorbisidec 3ds-jansson"
if ($LASTEXITCODE -ne 0) {
    throw "Nintendo 3DS dependency installation failed."
}

Write-Host "Windows setup completed." -ForegroundColor Green
Write-Host "Set app\config\server.mk before building the client." -ForegroundColor Cyan