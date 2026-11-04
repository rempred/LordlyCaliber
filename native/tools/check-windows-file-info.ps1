param(
    [Parameter(Mandatory)][string]$Directory,
    [Parameter(Mandatory)][string]$Version
)
$ErrorActionPreference = 'Stop'
$expected = [version]$Version
$descriptions = @{
    'LordlyCaliber' = 'Lordly Caliber ROM Editor'
    'LordlyCaliber-cli' = 'Lordly Caliber Command-line Tool'
}
foreach ($name in $descriptions.Keys) {
    $info = (Get-Item -LiteralPath (Join-Path $Directory "$name.exe")).VersionInfo
    $strings = @{
        ProductName = 'Lordly Caliber'; CompanyName = 'rempred'
        FileDescription = $descriptions[$name]; FileVersion = $Version; ProductVersion = $Version
        InternalName = $name; OriginalFilename = "$name.exe"
        LegalCopyright = 'Copyright (c) 2026 rempred'
    }
    foreach ($field in $strings.Keys) {
        if ($info.$field -cne $strings[$field]) { throw "$name Windows $field differs: '$($info.$field)'" }
    }
    foreach ($prefix in @('File', 'Product')) {
        if ($info."${prefix}MajorPart" -ne $expected.Major -or
            $info."${prefix}MinorPart" -ne $expected.Minor -or
            $info."${prefix}BuildPart" -ne $expected.Build -or
            $info."${prefix}PrivatePart" -ne 0) {
            throw "$name Windows numeric $prefix version differs from $Version.0"
        }
    }
}
Write-Output "PASS: GUI/CLI Windows file information matches $Version."
