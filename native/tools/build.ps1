param(
    [string]$ToolsRoot = "$env:USERPROFILE\.codex\lordly-native-toolchain",
    [ValidateSet('Release','Debug')][string]$Configuration = 'Release',
    [switch]$Test,
    [string]$RomPath = ''
)
$ErrorActionPreference = 'Stop'
$sourceRoot = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsRoot = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vsRoot) { throw 'MSVC C++ build tools are required.' }
$vcvars = Join-Path $vsRoot 'VC\Auxiliary\Build\vcvarsall.bat'
$environmentLines = & cmd.exe /d /s /c "`"$vcvars`" x64 -vcvars_ver=14.44 >nul && set"
if ($LASTEXITCODE -ne 0) { throw 'Cannot initialize the Qt-supported MSVC 2022 toolset.' }
foreach ($line in $environmentLines) {
    if ($line -match '^([^=]+)=(.*)$') { Set-Item -LiteralPath "Env:$($Matches[1])" -Value $Matches[2] }
}
$qtRoot = Join-Path $ToolsRoot 'Qt\6.12.0\msvc2022_64'
$cmake = Join-Path $ToolsRoot 'cmake-4.4.4-windows-x86_64\bin\cmake.exe'
$ninja = Join-Path $vsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
$buildRoot = Join-Path $sourceRoot "build\$Configuration"
& $cmake -S $sourceRoot -B $buildRoot -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_PREFIX_PATH=$qtRoot" "-DCMAKE_BUILD_TYPE=$Configuration"
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
& $cmake --build $buildRoot --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Native build failed.' }
if ($Test) {
    $env:PATH = "$qtRoot\bin;$env:PATH"
    $env:LORDLY_TEST_ROM = $RomPath
    $env:LORDLY_N64_TOOLCHAIN = Join-Path $sourceRoot '.local\n64-toolchain'
    $env:LORDLY_TEST_SCREENSHOT = Join-Path $buildRoot 'workspace.png'
    & (Join-Path (Split-Path $cmake) 'ctest.exe') --test-dir $buildRoot --output-on-failure
    $testExit = $LASTEXITCODE
    Get-Content -LiteralPath (Join-Path $buildRoot 'engine-test-report.txt')
    Get-Content -LiteralPath (Join-Path $buildRoot 'ui-test-report.txt') -ErrorAction SilentlyContinue
    if ($testExit -ne 0) { throw 'Native tests failed.' }
}
