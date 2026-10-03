param(
    [Parameter(Mandatory)][string]$Package,
    [Parameter(Mandatory)][string]$RomPath
)
$ErrorActionPreference = 'Stop'
$Package = (Resolve-Path -LiteralPath $Package).Path
$RomPath = (Resolve-Path -LiteralPath $RomPath).Path
$originalHash = (Get-FileHash -LiteralPath $RomPath -Algorithm SHA256).Hash
$runDirectory = Join-Path (Split-Path $RomPath) ('Test ROMs\native-portable-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $runDirectory | Out-Null
$project = Join-Path $runDirectory 'portable-edit.json'
$output = Join-Path $runDirectory (Split-Path $RomPath -Leaf)
$report = Join-Path $runDirectory 'export-report.json'
$inspection = Join-Path $runDirectory 'reopen-report.json'
@{
    format = 'ob64-patch'; version = 36
    patches = @{ classDefs = @{ '5' = @{ record_index = 6; bytes = @{ '0' = 1; '1' = 44 } } } }
} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $project -Encoding utf8
$saved = @{}
foreach ($key in @('PATH', 'QT_PLUGIN_PATH', 'QT_QPA_PLATFORM_PLUGIN_PATH', 'QTDIR', 'LORDLY_N64_TOOLCHAIN')) {
    $saved[$key] = [Environment]::GetEnvironmentVariable($key, 'Process')
    [Environment]::SetEnvironmentVariable($key, $null, 'Process')
}
try {
    $env:PATH = "$env:WINDIR\System32;$env:WINDIR"
    $gui = Start-Process -FilePath (Join-Path $Package 'LordlyCaliber.exe') -ArgumentList '--version' -WorkingDirectory $Package -WindowStyle Hidden -PassThru -Wait
    if ($gui.ExitCode -ne 0) { throw "Packaged Qt application failed: $($gui.ExitCode)" }
    & (Join-Path $Package 'LordlyCaliber-cli.exe') --rom $RomPath --project $project --output $output --report $report
    if ($LASTEXITCODE -ne 0) { throw 'Packaged export failed.' }
    & (Join-Path $Package 'LordlyCaliber-cli.exe') --rom $output --report $inspection
    if ($LASTEXITCODE -ne 0) { throw 'Packaged reopen failed.' }
    # Independent expected bytes: Knight STR = 300, big-endian 01 2C.
    $stream = [IO.File]::OpenRead($output)
    try {
        $null = $stream.Seek(0x5DAD8 + 6 * 72, [IO.SeekOrigin]::Begin)
        $first = $stream.ReadByte(); $second = $stream.ReadByte()
        $header = [IO.File]::ReadAllBytes($RomPath)[0..3]
        if ($header[0] -eq 0x37) { $actual = $second * 256 + $first }
        elseif ($header[0] -eq 0x80) { $actual = $first * 256 + $second }
        else { throw 'This byte-level package check supports z64/v64 source order.' }
        if ($actual -ne 300) { throw "Exported STR differs from authored value: $actual" }
    } finally { $stream.Dispose() }
    if ((Get-FileHash -LiteralPath $RomPath -Algorithm SHA256).Hash -ne $originalHash) { throw 'Source ROM changed.' }
    $summary = Get-Content -LiteralPath $inspection -Raw | ConvertFrom-Json
    if ($summary.classes -ne 165 -or $summary.items -ne 278) { throw 'Reopened catalog changed unexpectedly.' }
    Write-Output "PASS: isolated package startup, compile/export, independent record readback, reopen and source preservation. Evidence: $runDirectory"
} finally {
    foreach ($key in $saved.Keys) { [Environment]::SetEnvironmentVariable($key, $saved[$key], 'Process') }
}
