param([string]$Destination = '')
$ErrorActionPreference = 'Stop'
$sourceRoot = Split-Path -Parent $PSScriptRoot
if (!$Destination) { $Destination = Join-Path $sourceRoot '.local\dependency-sources' }
New-Item -ItemType Directory -Path $Destination -Force | Out-Null
$records = Get-Content -LiteralPath (Join-Path $sourceRoot 'rebuild\dependency-sources.json') -Raw | ConvertFrom-Json
foreach ($record in $records) {
    $file = Join-Path $Destination $record.file
    if (!(Test-Path -LiteralPath $file)) { Invoke-WebRequest -Uri $record.url -OutFile $file }
    if ((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $record.sha256) {
        throw "Dependency source hash mismatch: $($record.file)"
    }
}
Write-Output "Dependency sources verified in $Destination"
