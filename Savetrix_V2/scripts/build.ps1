$ErrorActionPreference = "Stop"

if (-not $env:VCPKG_ROOT) {
    throw "VCPKG_ROOT não está definido. Ex.: `$env:VCPKG_ROOT='C:\\dev\\vcpkg'"
}

Push-Location (Split-Path -Parent $PSScriptRoot)
try {
    cmake --preset release
    cmake --build --preset release
    Write-Host "DLL: build\\dist\\Data\\SKSE\\Plugins\\Savetrix.dll"
} finally {
    Pop-Location
}
