param(
    [Parameter(Mandatory)][string]$Package,
    [Parameter(Mandatory)][string]$RomPath
)
$ErrorActionPreference = 'Stop'
$Package = (Resolve-Path -LiteralPath $Package).Path
$RomPath = (Resolve-Path -LiteralPath $RomPath).Path
$nodeExecutable = (Get-Command node).Source
$nativeSource = Split-Path -Parent $PSScriptRoot
$runDirectory = Join-Path (Split-Path $RomPath) ('Test ROMs/portable-scene-import-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $runDirectory | Out-Null
$fixture = Join-Path $runDirectory 'reference.json'
$projectFile = Join-Path $runDirectory 'project.json'
$outputRom = Join-Path $runDirectory (Split-Path $RomPath -Leaf)
& $nodeExecutable (Join-Path $nativeSource 'tests/browser-dialogue-project.cjs') $RomPath $fixture
if ($LASTEXITCODE -ne 0) { throw 'Browser reference generation failed.' }
(Get-Content -LiteralPath $fixture -Raw | ConvertFrom-Json -Depth 100).project | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath $projectFile -Encoding utf8
$saved = @{}
foreach ($key in @('PATH','QT_PLUGIN_PATH','QT_QPA_PLATFORM_PLUGIN_PATH','QTDIR','QML_IMPORT_PATH','QML2_IMPORT_PATH','LORDLY_N64_TOOLCHAIN')) {
    $saved[$key] = [Environment]::GetEnvironmentVariable($key,'Process')
    [Environment]::SetEnvironmentVariable($key,$null,'Process')
}
try {
    $env:PATH = "$env:WINDIR/System32;$env:WINDIR"
    & (Join-Path $Package 'LordlyCaliber-cli.exe') --rom $RomPath --project $projectFile --output $outputRom --report (Join-Path $runDirectory 'export.json') | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Packaged browser-project import/export failed.' }
    & (Join-Path $Package 'LordlyCaliber-cli.exe') --rom $outputRom --directors --report (Join-Path $runDirectory 'reopen.json') | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Packaged imported-ROM reopen failed.' }
} finally {
    foreach ($key in $saved.Keys) { [Environment]::SetEnvironmentVariable($key,$saved[$key],'Process') }
}
& $nodeExecutable (Join-Path $nativeSource 'tests/verify-browser-scene-export.cjs') $outputRom $fixture
if ($LASTEXITCODE -ne 0) { throw 'Packaged scene bytes differ from the browser reference.' }
Write-Output "PASS: isolated portable scene conversion, user-time compilation, export and reopen. Evidence: $runDirectory"
