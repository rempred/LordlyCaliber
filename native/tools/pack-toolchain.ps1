param([Parameter(Mandatory)][string]$N64Tools)
$ErrorActionPreference = 'Stop'
$sourceRoot = Split-Path -Parent $PSScriptRoot
$pins = Get-Content -LiteralPath (Join-Path $sourceRoot 'rebuild/toolchain.json') -Raw | ConvertFrom-Json
$archivePath = Join-Path $sourceRoot 'rebuild/n64-toolchain-windows.zip'
$manifestPath = Join-Path $sourceRoot 'rebuild/toolchain-distribution.json'
$names = @($pins.tools.PSObject.Properties.Name | Sort-Object)
foreach ($name in $names) {
    if ((Get-FileHash -LiteralPath (Join-Path $N64Tools $name) -Algorithm SHA256).Hash -ne $pins.tools.$name) {
        throw "Tool identity mismatch: $name. Rebuild using rebuild/toolchain-source/README.md."
    }
}
Add-Type -AssemblyName System.IO.Compression
$stream = [IO.File]::Open($archivePath, [IO.FileMode]::Create, [IO.FileAccess]::Write)
try {
    $zip = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create, $true)
    try {
        foreach ($name in $names) {
            $entry = $zip.CreateEntry($name, [IO.Compression.CompressionLevel]::Optimal)
            $entry.LastWriteTime = [DateTimeOffset]::new(2000, 1, 1, 0, 0, 0, [TimeSpan]::Zero)
            $inputFile = [IO.File]::OpenRead((Join-Path $N64Tools $name))
            $outputFile = $entry.Open()
            try { $inputFile.CopyTo($outputFile) }
            finally { $inputFile.Dispose(); $outputFile.Dispose() }
        }
    } finally { $zip.Dispose() }
} finally { $stream.Dispose() }
$record = [ordered]@{
    schemaVersion = 1
    generator = 'native/tools/pack-toolchain.ps1'
    archive = 'n64-toolchain-windows.zip'
    sha256 = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
    toolchain = $pins.id
    inputs = 'native/rebuild/toolchain.json'
    sourcesAndRecipes = 'native/rebuild/toolchain-source/README.md'
}
[IO.File]::WriteAllText($manifestPath, ($record | ConvertTo-Json).Replace("`r`n", "`n") + "`n", [Text.UTF8Encoding]::new($false))
Write-Output "Packed $($names.Count) authenticated tools: $archivePath"
