$ErrorActionPreference = "Stop"

if (-not $env:VCPKG_ROOT) {
    throw "VCPKG_ROOT não está definido. Ex.: `$env:VCPKG_ROOT='C:\\dev\\vcpkg'"
}

Push-Location (Split-Path -Parent $PSScriptRoot)
try {
    cmake --preset release
    if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed." }
    cmake --build --preset release
    if ($LASTEXITCODE -ne 0) { throw "Savetrix build failed." }
    ctest --test-dir build -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "Savetrix regression tests failed." }
    python -m unittest discover -s tests -v
    if ($LASTEXITCODE -ne 0) { throw "Savetrix package/metadata tests failed." }
    $version = (Get-Content -LiteralPath "build/Savetrix.version" -Raw).Trim()
    python scripts/verify_dll.py build/dist/Data/SKSE/Plugins/Savetrix.dll --version $version
    if ($LASTEXITCODE -ne 0) { throw "Savetrix DLL validation failed." }
    Write-Host "DLL: build\\dist\\Data\\SKSE\\Plugins\\Savetrix.dll"
} finally {
    Pop-Location
}
