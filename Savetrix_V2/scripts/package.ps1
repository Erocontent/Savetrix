$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$dll = Join-Path $root "build\dist\Data\SKSE\Plugins\Savetrix.dll"
if (-not (Test-Path $dll)) { throw "Compile primeiro com scripts\build.ps1" }

$stage = Join-Path $root "package"
Remove-Item $stage -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path (Join-Path $stage "Data\SKSE\Plugins") -Force | Out-Null
Copy-Item $dll (Join-Path $stage "Data\SKSE\Plugins\Savetrix.dll")
Copy-Item (Join-Path $root "README.md") (Join-Path $stage "README.md")
Copy-Item (Join-Path $root "CHANGELOG.md") (Join-Path $stage "CHANGELOG.md")

$out = Join-Path $root "Savetrix_V2_Release.zip"
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $out -Force
Write-Host "Pacote criado: Savetrix_V2_Release.zip"
