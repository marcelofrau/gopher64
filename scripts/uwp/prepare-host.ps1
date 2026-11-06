[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [string]$DozenDir,
    [string]$PackageVersion
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'common.ps1')
$root = Resolve-UwpPhysicalPath (Join-Path $PSScriptRoot '../..')
$physicalOutput = Get-UwpHostOutputDir $root ''
$deps = Join-Path $root 'out/uwp-deps'
foreach ($file in @('lib/SDL3.lib', 'lib/SDL3_ttf.lib', 'bin/SDL3.dll', 'bin/SDL3_ttf.dll', 'include/SDL3/SDL.h')) {
    if (-not (Test-Path -LiteralPath (Join-Path $deps $file))) {
        throw "Missing UWP dependency $file. Run scripts/uwp/build-deps.ps1 first."
    }
}
if (-not $DozenDir) { $DozenDir = Join-Path $root 'out/uwp-dozen' }
foreach ($dll in @('vulkan_dzn.dll', 'dxil.dll')) {
    if (-not (Test-Path -LiteralPath (Join-Path $DozenDir $dll))) { throw "Missing $dll in Dozen directory: $DozenDir" }
}
$imgui = Join-Path $deps 'imgui'
if (-not (Test-Path -LiteralPath (Join-Path $imgui 'imgui.h'))) {
    $archive = Join-Path $deps 'imgui-b916186e66aa3c02b72c6ca17455516512ed8049.tar.gz'
    if (-not (Test-Path -LiteralPath $archive)) {
        Invoke-WebRequest 'https://github.com/ocornut/imgui/archive/b916186e66aa3c02b72c6ca17455516512ed8049.tar.gz' -OutFile $archive
    }
    if ((Get-FileHash $archive).Hash -ne 'A8E9ED7ADB544E2D528722E96A229272716FB9E220A4B9E2CEEE5469D548F562') { throw 'ImGui archive SHA256 mismatch.' }
    New-Item -ItemType Directory -Path $imgui -Force | Out-Null
    & tar -xzf $archive -C $imgui --strip-components=1
    if ($LASTEXITCODE) { throw 'ImGui extraction failed.' }
}
$output = Join-Path $root 'out/uwp-host'
$intermediate = Join-Path $output "obj/$Configuration"
New-Item -ItemType Directory -Path $intermediate -Force | Out-Null
Set-Content -LiteralPath (Join-Path $intermediate 'physical-output.txt') -Value (Join-Path $physicalOutput $Configuration) -Encoding utf8
$certificate = Join-Path $output 'Gopher64_TemporaryKey.pfx'
if (-not (Test-Path -LiteralPath $certificate)) {
    $cmakeInfo = & cmake --system-information
    if ($LASTEXITCODE) { throw 'Cannot find the CMake development certificate.' }
    $cmakeRoot = ($cmakeInfo | Select-String '^CMAKE_ROOT "(.+)"$').Matches.Groups[1].Value
    Copy-Item -LiteralPath (Join-Path $cmakeRoot 'Templates/Windows/Windows_TemporaryKey.pfx') -Destination $certificate
}
if (-not $PackageVersion) {
    $now = [DateTime]::UtcNow
    $days = [int]($now.Date - [DateTime]::new(2020, 1, 1)).TotalDays
    $PackageVersion = "0.$days.$($now.Hour * 60 + $now.Minute).$($now.Second * 1000 + $now.Millisecond)"
}
if ($PackageVersion -notmatch '^\d+\.\d+\.\d+\.\d+$' -or @($PackageVersion.Split('.') | Where-Object { [long]$_ -gt 65535 }).Count) {
    throw "Invalid four-part UWP package version: $PackageVersion"
}
$manifest = (Get-Content -LiteralPath (Join-Path $root 'uwp/Package.appxmanifest.in') -Raw).Replace('@GOPHER64_PACKAGE_VERSION@', $PackageVersion)
$path = Join-Path $intermediate 'Package.appxmanifest'
if (-not (Test-Path -LiteralPath $path) -or (Get-Content -LiteralPath $path -Raw) -ne $manifest) {
    Set-Content -LiteralPath $path -Value $manifest -NoNewline -Encoding utf8
}
Write-Host "UWP development package version: $PackageVersion"
