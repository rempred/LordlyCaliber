param([Parameter(Mandatory)][string]$Package)
$ErrorActionPreference = 'Stop'
$sourceRoot = Split-Path -Parent $PSScriptRoot
$repoRoot = Split-Path -Parent $sourceRoot
$Package = (Resolve-Path -LiteralPath $Package).Path
$commit = (& git -C $repoRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Cannot determine package source commit.' }
$sourceChanges = & git -C $repoRoot status --porcelain
if ($LASTEXITCODE -ne 0) { throw 'Cannot determine source status.' }
if ($env:GITHUB_ACTIONS -eq 'true' -and
    ($sourceChanges -or $commit -ne $env:GITHUB_SHA -or $env:GITHUB_REF -ne 'refs/heads/main')) {
    throw 'GitHub release packages require an unchanged checkout of the triggering Main commit.'
}
$dependencies = Get-Content -LiteralPath (Join-Path $sourceRoot 'dependencies.json') -Raw | ConvertFrom-Json
$metadata = [ordered]@{
    schemaVersion = 1
    sourceCommit = $commit
    sourceHasChanges = [bool]$sourceChanges
    repository = if ($env:GITHUB_ACTIONS -eq 'true') { $env:GITHUB_REPOSITORY } else { 'rempred/LordlyCaliber' }
    sourceRef = if ($env:GITHUB_ACTIONS -eq 'true') { $env:GITHUB_REF } else { (& git -C $repoRoot branch --show-current).Trim() }
    workflowRun = if ($env:GITHUB_ACTIONS -eq 'true') {
        "$env:GITHUB_SERVER_URL/$env:GITHUB_REPOSITORY/actions/runs/$env:GITHUB_RUN_ID/attempts/$env:GITHUB_RUN_ATTEMPT"
    } else { $null }
    runnerImage = $env:ImageVersion
    architecture = 'windows-x64'
    qt = $dependencies.qt.version
    cmake = $dependencies.cmake.version
    n64Toolchain = (Get-Content -LiteralPath (Join-Path $sourceRoot 'rebuild/toolchain.json') -Raw | ConvertFrom-Json).id
    dependencyPolicy = 'Editor binaries compiled from repository source; Qt and N64 export tools are authenticated binary dependencies with corresponding sources included.'
    testScope = 'Public CI does not use or distribute a private game ROM; ROM-dependent tests are skipped.'
}
[IO.File]::WriteAllText((Join-Path $Package 'build-info.json'), ($metadata | ConvertTo-Json -Depth 5) + "`n", [Text.UTF8Encoding]::new($false))
$manifest = Join-Path $Package 'FILES.sha256'
$rows = @(Get-ChildItem -LiteralPath $Package -File -Recurse | Where-Object { $_.FullName -ne $manifest } |
    Sort-Object FullName | ForEach-Object {
        $relative = [IO.Path]::GetRelativePath($Package, $_.FullName).Replace('\', '/')
        $hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        "$hash  $relative"
    })
[IO.File]::WriteAllText($manifest, ($rows -join "`n") + "`n", [Text.UTF8Encoding]::new($false))
Write-Output "Recorded source commit and $($rows.Count) package file checksums."
