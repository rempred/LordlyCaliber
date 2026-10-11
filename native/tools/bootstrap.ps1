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
$sevenZip = Get-Command 7z.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source -First 1
if (!$sevenZip -and (Test-Path -LiteralPath "$env:ProgramFiles/7-Zip/7z.exe")) {
    $sevenZip = "$env:ProgramFiles/7-Zip/7z.exe"
}
foreach ($record in $pins.qt.archives) {
    $name = ([Uri]$record.url).Segments[-1]
    $archive = Get-PinnedArchive $record.url $name $record.sha256
    $stamp = Join-Path $qtRoot ('.installed-' + $record.sha256)
    if (!(Test-Path -LiteralPath $stamp)) {
        # Server 2022's system tar cannot decode Qt's LZMA-compressed 7z files.
        if ($sevenZip) { & $sevenZip x -y "-o$qtRoot" $archive }
        else { & tar.exe -xf $archive -C $qtRoot }
        if ($LASTEXITCODE -ne 0) { throw "Could not extract $name. Install 7-Zip and retry." }
        Set-Content -LiteralPath $stamp -Value $record.sha256
    }
}
Set-Content -LiteralPath (Join-Path $qtRoot 'bin\qt.conf') -Value "[Paths]`nPrefix=.." -Encoding ascii
Write-Output "Pinned Qt and CMake are available at $ToolsRoot. Build with native/tools/build.ps1."
