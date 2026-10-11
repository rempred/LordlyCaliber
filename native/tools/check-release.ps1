param(
    [Parameter(Mandatory)][string]$Zip,
    [Parameter(Mandatory)][string]$ExpectedCommit,
    [switch]$RequireClean
)
$ErrorActionPreference = 'Stop'
$sourceRoot = Split-Path -Parent $PSScriptRoot
$Zip = (Resolve-Path -LiteralPath $Zip).Path
$zipName = [IO.Path]::GetFileName($Zip)
$packageName = [IO.Path]::GetFileNameWithoutExtension($Zip)
$checksum = (Get-Content -LiteralPath ($Zip + '.sha256') -Raw).Trim()
$actualZipHash = (Get-FileHash -LiteralPath $Zip -Algorithm SHA256).Hash.ToLowerInvariant()
if ($checksum -cne "$actualZipHash  $zipName") { throw 'Release ZIP checksum mismatch.' }
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::OpenRead($Zip)
try {
    $entries = @{}
    foreach ($entry in $archive.Entries) {
        $name = $entry.FullName.Replace('\', '/')
        if ((!$name.StartsWith($packageName + '/', [StringComparison]::Ordinal) -and $name -cne 'attestations.jsonl') -or
            $name -match '(^|/)\.\.?(/|$)|:' -or $entries.ContainsKey($name)) {
            throw "Unexpected release archive path: $name"
        }
        $entries[$name] = $true
    }
} finally { $archive.Dispose() }
$checkRoot = Join-Path $sourceRoot ('.local/release-check-' + [guid]::NewGuid().ToString('N'))
[IO.Compression.ZipFile]::ExtractToDirectory($Zip, $checkRoot)
$package = Join-Path $checkRoot $packageName
$metadata = Get-Content -LiteralPath (Join-Path $package 'build-info.json') -Raw | ConvertFrom-Json
if ($metadata.sourceCommit -cne $ExpectedCommit) { throw 'Release source commit mismatch.' }
if ($metadata.schemaVersion -ne 1 -or $metadata.sourceHasChanges -isnot [bool]) { throw 'Invalid release metadata.' }
if ($RequireClean -and $metadata.sourceHasChanges) { throw 'Release was made from modified source.' }
$files = @(Get-ChildItem -LiteralPath $package -File -Recurse)
$recorded = @{}
foreach ($line in (Get-Content -LiteralPath (Join-Path $package 'FILES.sha256'))) {
    if ($line -cnotmatch '^([0-9a-f]{64})  (.+)$') { throw 'Malformed package file checksum.' }
    $digest = $Matches[1]; $name = $Matches[2]
    if ($name -match '(^|/)\.\.?(/|$)|:|\\' -or $name.StartsWith('/') -or $recorded.ContainsKey($name)) {
        throw "Unexpected package manifest path: $name"
    }
    $path = Join-Path $package $name
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $digest) { throw "Package checksum mismatch: $name" }
    $recorded[$name] = $true
}
if ($recorded.Count -ne $files.Count - 1 -or $recorded.ContainsKey('FILES.sha256')) {
    throw 'Package manifest does not cover exactly the shipped files.'
}
$required = @('LordlyCaliber.exe', 'LordlyCaliber-cli.exe', 'qt.conf', 'Qt6Core.dll', 'Qt6Gui.dll',
    'Qt6Widgets.dll', 'Qt6Qml.dll', 'Qt6Network.dll', 'Qt6Svg.dll', 'msvcp140.dll', 'vcruntime140.dll',
    'platforms/qwindows.dll', 'imageformats/qgif.dll', 'imageformats/qjpeg.dll', 'imageformats/qsvg.dll',
    'iconengines/qsvgicon.dll', 'styles/qmodernwindowsstyle.dll', 'README.md', 'THIRD-PARTY.md', 'VERIFY-BUILD.md',
    'licenses/LordlyCaliber.txt', 'game/README.md', 'dependencies.json', 'build-info.json',
    'dependency-sources/toolchain.json', 'dependency-sources/toolchain-distribution.json',
    'dependency-sources/toolchain-source/README.md', 'dependency-sources/dependency-sources.json')
foreach ($name in $required) {
    if (!$recorded.ContainsKey($name)) { throw "Missing release component: $name" }
}
if ($files | Where-Object { $_.Extension -in @('.v64', '.z64', '.n64') }) { throw 'A release must not contain ROM files.' }
foreach ($pin in (Get-Content -LiteralPath (Join-Path $sourceRoot 'rebuild/toolchain.json') -Raw | ConvertFrom-Json).tools.PSObject.Properties) {
    if ((Get-FileHash -LiteralPath (Join-Path $package "n64-toolchain/$($pin.Name)") -Algorithm SHA256).Hash -ne $pin.Value) {
        throw "Packaged N64 tool identity mismatch: $($pin.Name)"
    }
}
foreach ($record in (Get-Content -LiteralPath (Join-Path $sourceRoot 'rebuild/dependency-sources.json') -Raw | ConvertFrom-Json)) {
    if ((Get-FileHash -LiteralPath (Join-Path $package "dependency-sources/$($record.file)") -Algorithm SHA256).Hash -ne $record.sha256) {
        throw "Packaged dependency source mismatch: $($record.file)"
    }
}
$saved = @{}
foreach ($key in @('PATH', 'QT_PLUGIN_PATH', 'QT_QPA_PLATFORM_PLUGIN_PATH', 'QT_QPA_PLATFORM', 'QTDIR',
                   'LORDLY_N64_TOOLCHAIN', 'QT_COMMAND_LINE_PARSER_NO_GUI_MESSAGE_BOXES')) {
    $saved[$key] = [Environment]::GetEnvironmentVariable($key, 'Process')
    [Environment]::SetEnvironmentVariable($key, $null, 'Process')
}
try {
    $env:PATH = "$env:WINDIR\System32;$env:WINDIR"
    $env:QT_COMMAND_LINE_PARSER_NO_GUI_MESSAGE_BOXES = '1'
    $versions = @{}
    foreach ($name in @('LordlyCaliber', 'LordlyCaliber-cli')) {
        $output = Join-Path $checkRoot "$name-version.txt"
        $errors = Join-Path $checkRoot "$name-errors.txt"
        $process = Start-Process -FilePath (Join-Path $package "$name.exe") -ArgumentList '--version' -WorkingDirectory $package -WindowStyle Hidden -PassThru -RedirectStandardOutput $output -RedirectStandardError $errors
        if (!$process.WaitForExit(30000)) {
            Stop-Process -Id $process.Id
            throw "$name startup timed out. See $errors"
        }
        $process.WaitForExit()
        if ($process.ExitCode -ne 0) { throw "$name startup failed: $(Get-Content -LiteralPath $errors -Raw)" }
        $versions[$name] = (Get-Content -LiteralPath $output -Raw).Trim()
        if (!$versions[$name].StartsWith("$name ")) { throw "Unexpected $name version output." }
    }
    if ($versions['LordlyCaliber'] -cne $versions['LordlyCaliber-cli'].Replace('LordlyCaliber-cli ', 'LordlyCaliber ')) {
        throw 'GUI and CLI versions do not match.'
    }
    & (Join-Path $package 'n64-toolchain/mips-kmc-elf-as.exe') --version | Select-Object -First 1
    if ($LASTEXITCODE -ne 0) { throw 'Packaged assembler could not start.' }
} finally {
    foreach ($key in $saved.Keys) { [Environment]::SetEnvironmentVariable($key, $saved[$key], 'Process') }
}
Write-Output "PASS: ZIP integrity, $($recorded.Count) files, dependency identities, source commit and isolated GUI/CLI/tool startup. Checked package: $package"
