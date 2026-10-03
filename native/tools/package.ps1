param(
    [string]$ToolsRoot = "$env:USERPROFILE\.codex\lordly-native-toolchain",
    [string]$N64Tools = '',
    [string]$Destination = ''
)
$ErrorActionPreference = 'Stop'
$sourceRoot = Split-Path -Parent $PSScriptRoot
if (!$N64Tools) { $N64Tools = Join-Path $sourceRoot '.local\n64-toolchain' }
if (!$Destination) { $Destination = Join-Path $sourceRoot ('dist\LordlyCaliber-0.5-dev-' + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
$Destination = [IO.Path]::GetFullPath($Destination)
if (Test-Path -LiteralPath $Destination) { throw 'Choose a fresh package directory.' }
$buildRoot = Join-Path $sourceRoot 'build\Release'
foreach ($name in @('LordlyCaliber.exe', 'LordlyCaliber-cli.exe')) {
    if (!(Test-Path -LiteralPath (Join-Path $buildRoot $name))) { throw "Build $name first." }
}
$pins = Get-Content -LiteralPath (Join-Path $sourceRoot 'rebuild\toolchain.json') -Raw | ConvertFrom-Json
foreach ($pin in $pins.tools.PSObject.Properties) {
    if ((Get-FileHash -LiteralPath (Join-Path $N64Tools $pin.Name) -Algorithm SHA256).Hash -ne $pin.Value) {
        throw "N64 tool identity mismatch: $($pin.Name)"
    }
}
& (Join-Path $PSScriptRoot 'fetch-sources.ps1')
New-Item -ItemType Directory -Path $Destination | Out-Null
foreach ($name in @('LordlyCaliber.exe', 'LordlyCaliber-cli.exe')) {
    Copy-Item -LiteralPath (Join-Path $buildRoot $name) -Destination $Destination
}
$qtRoot = Join-Path $ToolsRoot 'Qt\6.12.0\msvc2022_64'
$priorPath = $env:PATH
try {
    $env:PATH = "$qtRoot\bin;$env:PATH"
    & (Join-Path $qtRoot 'bin\windeployqt.exe') --release --no-compiler-runtime --no-translations --no-opengl-sw --no-system-d3d-compiler --no-system-dxc-compiler --skip-plugin-types generic,networkinformation,tls --dir $Destination (Join-Path $Destination 'LordlyCaliber.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Qt deployment failed.' }
} finally { $env:PATH = $priorPath }
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsRoot = & $vswhere -latest -products '*' -property installationPath
$redist = Join-Path $vsRoot 'VC\Redist\MSVC\14.51.36231\x64\Microsoft.VC145.CRT'
if (!(Test-Path -LiteralPath $redist)) { throw 'Required app-local Microsoft redistributable directory is missing.' }
Get-ChildItem -LiteralPath $redist -File -Filter '*.dll' | Copy-Item -Destination $Destination
$toolDest = New-Item -ItemType Directory -Path (Join-Path $Destination 'n64-toolchain')
foreach ($pin in $pins.tools.PSObject.Properties) { Copy-Item -LiteralPath (Join-Path $N64Tools $pin.Name) -Destination $toolDest }
Copy-Item -LiteralPath (Join-Path $sourceRoot 'game') -Destination $Destination -Recurse
Copy-Item -LiteralPath (Join-Path $sourceRoot 'licenses') -Destination $Destination -Recurse
Copy-Item -LiteralPath (Join-Path (Split-Path $sourceRoot) 'LICENSE') -Destination (Join-Path $Destination 'licenses\LordlyCaliber.txt')
Copy-Item -LiteralPath (Join-Path $sourceRoot 'README.md') -Destination $Destination
Copy-Item -LiteralPath (Join-Path $sourceRoot 'THIRD-PARTY.md') -Destination $Destination
Copy-Item -LiteralPath (Join-Path $sourceRoot 'dependencies.json') -Destination $Destination
$sources = New-Item -ItemType Directory -Path (Join-Path $Destination 'dependency-sources')
foreach ($record in (Get-Content -LiteralPath (Join-Path $sourceRoot 'rebuild\dependency-sources.json') -Raw | ConvertFrom-Json)) {
    Copy-Item -LiteralPath (Join-Path $sourceRoot ('.local\dependency-sources\' + $record.file)) -Destination $sources
}
Copy-Item -LiteralPath (Join-Path $sourceRoot 'rebuild\toolchain-source') -Destination $sources -Recurse
Copy-Item -LiteralPath (Join-Path $sourceRoot 'rebuild\dependency-sources.json') -Destination $sources
Set-Content -LiteralPath (Join-Path $Destination 'qt.conf') -Value "[Paths]`nPrefix=." -Encoding ascii
Compress-Archive -LiteralPath $Destination -DestinationPath ($Destination + '.zip') -CompressionLevel Fastest
Write-Output "Portable development package: $Destination.zip"
