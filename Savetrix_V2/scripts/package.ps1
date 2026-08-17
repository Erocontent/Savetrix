$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$dll = Join-Path $root "build\dist\Data\SKSE\Plugins\Savetrix.dll"
if (-not (Test-Path $dll)) { throw "Compile first with the normal Savetrix build." }

$stage = Join-Path $root "package"
Remove-Item $stage -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path (Join-Path $stage "Data\SKSE\Plugins") -Force | Out-Null
Copy-Item $dll (Join-Path $stage "Data\SKSE\Plugins\Savetrix.dll")

$mcmSource = Join-Path $root "Data\MCM"
if (Test-Path $mcmSource) {
    New-Item -ItemType Directory -Path (Join-Path $stage "Data") -Force | Out-Null
    Copy-Item $mcmSource (Join-Path $stage "Data\MCM") -Recurse -Force
}

$optionalPlugin = Join-Path $root "Data\Savetrix.esp"
if (Test-Path $optionalPlugin) {
    Copy-Item $optionalPlugin (Join-Path $stage "Data\Savetrix.esp")
}

$out = Join-Path $root "Savetrix_V2_Release.zip"
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $out -Force
Write-Host "Package created: Savetrix_V2_Release.zip"
