param([string]$ToolsRoot = "$env:USERPROFILE\.codex\lordly-native-toolchain")
$ErrorActionPreference = 'Stop'
$sourceRoot = Split-Path -Parent $PSScriptRoot
$pins = Get-Content -LiteralPath (Join-Path $sourceRoot 'dependencies.json') -Raw | ConvertFrom-Json
New-Item -ItemType Directory -Path $ToolsRoot -Force | Out-Null
function Get-PinnedArchive([string]$Url, [string]$Name, [string]$Sha256) {
    $file = Join-Path $ToolsRoot $Name
    if (!(Test-Path -LiteralPath $file)) { Invoke-WebRequest -Uri $Url -OutFile $file }
    if ((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $Sha256) {
        throw "Dependency hash mismatch: $Name"
    }
    return $file
}
$cmakeArchive = Get-PinnedArchive $pins.cmake.url "cmake-$($pins.cmake.version)-windows-x86_64.zip" $pins.cmake.sha256
if (!(Test-Path -LiteralPath (Join-Path $ToolsRoot "cmake-$($pins.cmake.version)-windows-x86_64\bin\cmake.exe"))) {
    Expand-Archive -LiteralPath $cmakeArchive -DestinationPath $ToolsRoot
}
$qtRoot = Join-Path $ToolsRoot "Qt\$($pins.qt.version)\msvc2022_64"
New-Item -ItemType Directory -Path $qtRoot -Force | Out-Null
foreach ($record in $pins.qt.archives) {
    $name = ([Uri]$record.url).Segments[-1]
    $archive = Get-PinnedArchive $record.url $name $record.sha256
    $stamp = Join-Path $qtRoot ('.installed-' + $record.sha256)
    if (!(Test-Path -LiteralPath $stamp)) {
        & tar.exe -xf $archive -C $qtRoot
        if ($LASTEXITCODE -ne 0) { throw "Could not extract $name" }
        Set-Content -LiteralPath $stamp -Value $record.sha256
    }
}
Set-Content -LiteralPath (Join-Path $qtRoot 'bin\qt.conf') -Value "[Paths]`nPrefix=.." -Encoding ascii
Write-Output "Pinned Qt and CMake are available at $ToolsRoot. Build with native/tools/build.ps1."
