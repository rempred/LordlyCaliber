param([string]$Destination = '')
$ErrorActionPreference = 'Stop'
$sourceRoot = Split-Path -Parent $PSScriptRoot
if (!$Destination) { $Destination = Join-Path $sourceRoot '.local/n64-toolchain' }
$pins = Get-Content -LiteralPath (Join-Path $sourceRoot 'rebuild/toolchain.json') -Raw | ConvertFrom-Json
$distribution = Get-Content -LiteralPath (Join-Path $sourceRoot 'rebuild/toolchain-distribution.json') -Raw | ConvertFrom-Json
if ($distribution.schemaVersion -ne 1 -or $distribution.toolchain -ne $pins.id -or
    $distribution.archive -ne 'n64-toolchain-windows.zip') { throw 'Unexpected toolchain distribution manifest.' }
$archivePath = Join-Path $sourceRoot ('rebuild/' + $distribution.archive)
if ((Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash -ne $distribution.sha256) {
    throw 'N64 toolchain archive checksum mismatch.'
}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [IO.Compression.ZipFile]::OpenRead($archivePath)
try {
    $names = @($pins.tools.PSObject.Properties.Name)
    if ($zip.Entries.Count -ne $names.Count) { throw 'Unexpected toolchain archive contents.' }
    $validated = @{}
    foreach ($entry in $zip.Entries) {
        if ($entry.FullName -cnotin $names -or $validated.ContainsKey($entry.FullName)) {
            throw "Unexpected toolchain archive entry: $($entry.FullName)"
        }
        $inputFile = $entry.Open()
        $hash = [Security.Cryptography.SHA256]::Create()
        try { $actual = [BitConverter]::ToString($hash.ComputeHash($inputFile)).Replace('-', '') }
        finally { $inputFile.Dispose(); $hash.Dispose() }
        if ($actual -ne $pins.tools.($entry.FullName)) { throw "N64 tool identity mismatch: $($entry.FullName)" }
        $validated[$entry.FullName] = $entry
    }
    # Validate the entire archive before writing anything to the destination.
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    foreach ($name in $names) {
        $target = Join-Path $Destination $name
        if ((Test-Path -LiteralPath $target) -and
            (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -eq $pins.tools.$name) { continue }
        [IO.Compression.ZipFileExtensions]::ExtractToFile($validated[$name], $target, $true)
    }
} finally { $zip.Dispose() }
Write-Output "Authenticated N64 export tools are available at $Destination"
