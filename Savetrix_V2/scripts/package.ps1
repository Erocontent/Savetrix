param(
    [string]$BuildDirectory = "build",
    [string]$PythonExecutable = "python"
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$build = [IO.Path]::GetFullPath((Join-Path $root $BuildDirectory))
$dll = Join-Path $build "dist\Data\SKSE\Plugins\Savetrix.dll"
if (-not (Test-Path -LiteralPath $dll)) { throw "Compile first with the normal Savetrix build." }
$version = (Get-Content -LiteralPath (Join-Path $build "Savetrix.version") -Raw).Trim()
& $PythonExecutable (Join-Path $PSScriptRoot "verify_dll.py") $dll --version $version
if ($LASTEXITCODE -ne 0) { throw "Refusing to package a DLL with invalid SKSE metadata." }

$stage = [IO.Path]::GetFullPath((Join-Path $root "package"))
if ($stage -ne (Join-Path $root "package")) { throw "Invalid staging path." }
if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
New-Item -ItemType Directory -Path (Join-Path $stage "Data\SKSE\Plugins") -Force | Out-Null
Copy-Item -LiteralPath $dll -Destination (Join-Path $stage "Data\SKSE\Plugins\Savetrix.dll")
Copy-Item -LiteralPath (Join-Path $root "Data\MCM") -Destination (Join-Path $stage "Data\MCM") -Recurse
Copy-Item -LiteralPath (Join-Path $root "Data\Savetrix.esp") -Destination (Join-Path $stage "Data\Savetrix.esp")
Copy-Item -LiteralPath (Join-Path $root "README.md"),(Join-Path $root "CHANGELOG.md"),(Join-Path $root "LICENSE") -Destination $stage
$out = Join-Path $root "Savetrix_V2_Release.zip"
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $out -Force
Get-FileHash -LiteralPath $out -Algorithm SHA256 | Format-List
Write-Host "Package created: Savetrix_V2_Release.zip ($version)"
