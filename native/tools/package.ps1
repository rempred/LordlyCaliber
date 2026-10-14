param(
    [string]$ToolsRoot = "$env:USERPROFILE\.codex\lordly-native-toolchain",
    [string]$N64Tools = '',
    [string]$Destination = '',
    [switch]$Live
)
$ErrorActionPreference = 'Stop'
$sourceRoot = Split-Path -Parent $PSScriptRoot
if (!$N64Tools) { $N64Tools = Join-Path $sourceRoot '.local\n64-toolchain' }
if (!$Destination) {
    $Destination = if ($Live) { Join-Path $sourceRoot 'live' }
                   else { Join-Path $sourceRoot ('dist\LordlyCaliber-0.5.1-' + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
}
$Destination = [IO.Path]::GetFullPath($Destination)
if ((Test-Path -LiteralPath $Destination) -and !$Live) { throw 'Choose a fresh package directory.' }
if ($Live) {
    $liveExe = Join-Path $Destination 'LordlyCaliber.exe'
    if (Get-Process -Name LordlyCaliber -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $liveExe }) {
        throw 'Save your work and close the live editor before updating it. No running editor was stopped.'
    }
}
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
New-Item -ItemType Directory -Path $Destination -Force | Out-Null
foreach ($name in @('LordlyCaliber.exe', 'LordlyCaliber-cli.exe')) {
    Copy-Item -LiteralPath (Join-Path $buildRoot $name) -Destination $Destination
}
$qtRoot = Join-Path $ToolsRoot 'Qt\6.12.0\msvc2022_64'
$priorPath = $env:PATH
try {
    $env:PATH = "$qtRoot\bin;$env:PATH"
    & (Join-Path $qtRoot 'bin\windeployqt.exe') --release --no-compiler-runtime --no-translations --no-opengl-sw --no-system-d3d-compiler --no-system-dxc-compiler --no-quick-import --skip-plugin-types generic,networkinformation,tls,qmltooling --dir $Destination (Join-Path $Destination 'LordlyCaliber.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Qt deployment failed.' }
} finally { $env:PATH = $priorPath }
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsRoot = & $vswhere -latest -products '*' -property installationPath
$redist = $null
foreach ($version in (Get-ChildItem -LiteralPath (Join-Path $vsRoot 'VC/Redist/MSVC') -Directory |
        Where-Object { $_.Name -match '^\d+\.\d+\.\d+' } | Sort-Object { [version]$_.Name } -Descending)) {
    $architecture = Join-Path $version.FullName 'x64'
    if (!(Test-Path -LiteralPath $architecture)) { continue }
    $candidate = Get-ChildItem -LiteralPath $architecture -Directory -Filter 'Microsoft.VC*.CRT' |
        Where-Object { (Test-Path -LiteralPath (Join-Path $_.FullName 'vcruntime140.dll')) -and
                      (Test-Path -LiteralPath (Join-Path $_.FullName 'msvcp140.dll')) } | Select-Object -First 1
    if ($candidate) { $redist = $candidate.FullName; break }
}
if (!$redist) { throw 'Required x64 Microsoft redistributable directory is missing.' }
Get-ChildItem -LiteralPath $redist -File -Filter '*.dll' | Copy-Item -Destination $Destination
$toolDest = New-Item -ItemType Directory -Path (Join-Path $Destination 'n64-toolchain') -Force
foreach ($pin in $pins.tools.PSObject.Properties) { Copy-Item -LiteralPath (Join-Path $N64Tools $pin.Name) -Destination $toolDest }
Copy-Item -LiteralPath (Join-Path $sourceRoot 'game') -Destination $Destination -Recurse -Force
Copy-Item -LiteralPath (Join-Path $sourceRoot 'licenses') -Destination $Destination -Recurse -Force
Copy-Item -LiteralPath (Join-Path (Split-Path $sourceRoot) 'LICENSE') -Destination (Join-Path $Destination 'licenses\LordlyCaliber.txt')
Copy-Item -LiteralPath (Join-Path $sourceRoot 'README.md') -Destination $Destination
Copy-Item -LiteralPath (Join-Path $sourceRoot 'THIRD-PARTY.md') -Destination $Destination
Copy-Item -LiteralPath (Join-Path $sourceRoot 'VERIFY-BUILD.md') -Destination $Destination
Copy-Item -LiteralPath (Join-Path $sourceRoot 'dependencies.json') -Destination $Destination
$sources = New-Item -ItemType Directory -Path (Join-Path $Destination 'dependency-sources') -Force
foreach ($record in (Get-Content -LiteralPath (Join-Path $sourceRoot 'rebuild\dependency-sources.json') -Raw | ConvertFrom-Json)) {
    Copy-Item -LiteralPath (Join-Path $sourceRoot ('.local\dependency-sources\' + $record.file)) -Destination $sources
}
Copy-Item -LiteralPath (Join-Path $sourceRoot 'rebuild\toolchain-source') -Destination $sources -Recurse -Force
Copy-Item -LiteralPath (Join-Path $sourceRoot 'rebuild\dependency-sources.json') -Destination $sources
foreach ($name in @('toolchain.json', 'toolchain-distribution.json', 'TOOLCHAIN-DISTRIBUTION.md')) {
    Copy-Item -LiteralPath (Join-Path $sourceRoot "rebuild/$name") -Destination $sources
}
Set-Content -LiteralPath (Join-Path $Destination 'qt.conf') -Value "[Paths]`nPrefix=." -Encoding ascii
if ($Live) { Write-Output "Live editor: $Destination\LordlyCaliber.exe" }
else {
    & (Join-Path $PSScriptRoot 'write-package-manifest.ps1') -Package $Destination
    Compress-Archive -LiteralPath $Destination -DestinationPath ($Destination + '.zip') -CompressionLevel Fastest
    $zipHash = (Get-FileHash -LiteralPath ($Destination + '.zip') -Algorithm SHA256).Hash.ToLowerInvariant()
    [IO.File]::WriteAllText(($Destination + '.zip.sha256'), "$zipHash  $([IO.Path]::GetFileName($Destination)).zip`n", [Text.UTF8Encoding]::new($false))
    Write-Output "Portable development package: $Destination.zip"
}
