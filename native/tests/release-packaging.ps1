param([string]$Zip = '')
$ErrorActionPreference = 'Stop'
$sourceRoot = Split-Path -Parent $PSScriptRoot
$scratch = Join-Path $sourceRoot ('.local/release-negative-' + [guid]::NewGuid().ToString('N'))
$fixture = Join-Path $scratch 'native'
New-Item -ItemType Directory -Path "$fixture/tools", "$fixture/rebuild" -Force | Out-Null
Copy-Item -LiteralPath "$sourceRoot/tools/bootstrap-n64.ps1" -Destination "$fixture/tools/"
foreach ($name in @('toolchain.json', 'toolchain-distribution.json', 'n64-toolchain-windows.zip')) {
    Copy-Item -LiteralPath "$sourceRoot/rebuild/$name" -Destination "$fixture/rebuild/"
}
function Expect-Rejection([scriptblock]$Action, [string]$Message) {
    $rejected = $false
    try { & $Action | Out-Null }
    catch {
        if ($_.Exception.Message -notlike "*$Message*") { throw }
        $rejected = $true
    }
    if (!$rejected) { throw "Expected rejection: $Message" }
    Write-Output "PASS: rejects $Message"
}
$output = Join-Path $scratch 'installed'
& "$fixture/tools/bootstrap-n64.ps1" -Destination $output
& "$fixture/tools/bootstrap-n64.ps1" -Destination $output
$pins = Get-Content -LiteralPath "$fixture/rebuild/toolchain.json" -Raw | ConvertFrom-Json
foreach ($pin in $pins.tools.PSObject.Properties) {
    if ((Get-FileHash -LiteralPath (Join-Path $output $pin.Name) -Algorithm SHA256).Hash -ne $pin.Value) {
        throw 'Valid toolchain bootstrap changed an authenticated tool.'
    }
}
Write-Output 'PASS: clean and repeated bootstrap preserve the authenticated tools.'
$archivePath = "$fixture/rebuild/n64-toolchain-windows.zip"
$bytes = [IO.File]::ReadAllBytes($archivePath)
$bytes[0] = $bytes[0] -bxor 1
[IO.File]::WriteAllBytes($archivePath, $bytes)
$badOutput = Join-Path $scratch 'rejected'
Expect-Rejection { & "$fixture/tools/bootstrap-n64.ps1" -Destination $badOutput } 'archive checksum mismatch'
if (Test-Path -LiteralPath $badOutput) { throw 'Rejected archive created an installation directory.' }
Copy-Item -LiteralPath "$sourceRoot/rebuild/n64-toolchain-windows.zip" -Destination $archivePath -Force
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::Open($archivePath, [IO.Compression.ZipArchiveMode]::Update)
try {
    $entry = $archive.GetEntry('cpp.exe'); $entry.Delete()
    $entry = $archive.CreateEntry('cpp.exe')
    $stream = $entry.Open()
    try { $stream.WriteByte(0) } finally { $stream.Dispose() }
} finally { $archive.Dispose() }
$distributionPath = "$fixture/rebuild/toolchain-distribution.json"
$distribution = Get-Content -LiteralPath $distributionPath -Raw | ConvertFrom-Json
$distribution.sha256 = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
$distribution | ConvertTo-Json | Set-Content -LiteralPath $distributionPath -Encoding utf8
Expect-Rejection { & "$fixture/tools/bootstrap-n64.ps1" -Destination $badOutput } 'tool identity mismatch'
if (Test-Path -LiteralPath $badOutput) { throw 'Rejected tool created an installation directory.' }

if ($Zip) {
    $Zip = (Resolve-Path -LiteralPath $Zip).Path
    $candidate = Join-Path $scratch ([IO.Path]::GetFileName($Zip))
    [IO.File]::WriteAllText($candidate, 'damaged ZIP')
    Copy-Item -LiteralPath ($Zip + '.sha256') -Destination ($candidate + '.sha256')
    $expectedCommit = (& git -C $sourceRoot rev-parse HEAD).Trim()
    Expect-Rejection { & "$sourceRoot/tools/check-release.ps1" -Zip $candidate -ExpectedCommit $expectedCommit } 'ZIP checksum mismatch'
    Copy-Item -LiteralPath $Zip -Destination $candidate -Force
    $archive = [IO.Compression.ZipFile]::Open($candidate, [IO.Compression.ZipArchiveMode]::Update)
    $packageName = [IO.Path]::GetFileNameWithoutExtension($Zip)
    try {
        $archive.GetEntry("$packageName/platforms/qwindows.dll").Delete()
        $entry = $archive.GetEntry("$packageName/FILES.sha256")
        $reader = [IO.StreamReader]::new($entry.Open())
        try { $manifest = $reader.ReadToEnd() } finally { $reader.Dispose() }
        $entry.Delete()
        $entry = $archive.CreateEntry("$packageName/FILES.sha256")
        $writer = [IO.StreamWriter]::new($entry.Open(), [Text.UTF8Encoding]::new($false))
        try {
            $manifest = (($manifest -split "`n" | Where-Object { $_ -notmatch '  platforms/qwindows.dll$' }) -join "`n")
            $writer.Write($manifest)
        } finally { $writer.Dispose() }
    } finally { $archive.Dispose() }
    $hash = (Get-FileHash -LiteralPath $candidate -Algorithm SHA256).Hash.ToLowerInvariant()
    [IO.File]::WriteAllText(($candidate + '.sha256'), "$hash  $([IO.Path]::GetFileName($candidate))`n", [Text.UTF8Encoding]::new($false))
    Expect-Rejection { & "$sourceRoot/tools/check-release.ps1" -Zip $candidate -ExpectedCommit $expectedCommit } 'Missing release component: platforms/qwindows.dll'
}
Write-Output "Release validation checks passed. Temporary fixtures: $scratch"
