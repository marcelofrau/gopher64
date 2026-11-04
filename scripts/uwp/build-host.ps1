[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [switch]$SkipRust,
    [switch]$SkipDependencies,
    [switch]$InitSubmodules,
    [string]$PackageVersion
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'common.ps1')
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$vs = Get-UwpVisualStudio
$build = Join-Path $root "out/uwp-host-vs$($vs.Version)"
if (-not $PackageVersion) {
    $now = [DateTime]::UtcNow
    $days = [int]($now.Date - [DateTime]::new(2020, 1, 1)).TotalDays
    $PackageVersion = "0.$days.$($now.Hour * 60 + $now.Minute).$($now.Second * 1000 + $now.Millisecond)"
}
if ($PackageVersion -notmatch '^\d+\.\d+\.\d+\.\d+$' -or
    @($PackageVersion.Split('.') | Where-Object { [long]$_ -gt 65535 }).Count) {
    throw "Invalid four-part UWP package version: $PackageVersion"
}

if (-not $SkipRust) {
    & (Join-Path $PSScriptRoot 'build.ps1') -Configuration $Configuration `
        -SkipDependencies:$SkipDependencies -InitSubmodules:$InitSubmodules
}
Write-Host "Configuring UWP host: $($vs.Generator) ($($vs.Install))"
Write-Host "Development package version: $PackageVersion"
& cmake -S (Join-Path $root 'uwp') -B $build -G $vs.Generator -A x64 `
    '-DCMAKE_SYSTEM_NAME=WindowsStore' '-DCMAKE_SYSTEM_VERSION=10.0' "-DGOPHER64_PACKAGE_VERSION=$PackageVersion"
if ($LASTEXITCODE) { throw "UWP host configuration failed ($LASTEXITCODE)." }
& cmake --build $build --config $Configuration --target gopher64-uwp --parallel
if ($LASTEXITCODE) { throw "UWP host build failed ($LASTEXITCODE)." }
$suffix = if ($Configuration -eq 'Debug') { '_x64_Debug' } else { '_x64' }
$packageBase = "gopher64-uwp_${PackageVersion}${suffix}"
$package = Join-Path $build "AppPackages/gopher64-uwp/${packageBase}_Test/$packageBase.msix"
if (-not (Test-Path -LiteralPath $package)) { throw "Expected UWP package not found: $package" }
& (Join-Path $PSScriptRoot 'audit-imports.ps1') -Package $package
# A previous VS deployment can have newer Debug DLLs in AppX than the Release
# dependency build; refresh only these generated files to prevent stale F5 payloads.
$layout = Join-Path $build "$Configuration/AppX"
if (Test-Path -LiteralPath $layout) {
    foreach ($dll in @('SDL3.dll', 'SDL3_ttf.dll')) {
        $source = Join-Path $root "out/uwp-deps/bin/$dll"
        $destination = Join-Path $layout $dll
        if (-not (Test-Path -LiteralPath $destination) -or
            (Get-FileHash -LiteralPath $source).Hash -ne (Get-FileHash -LiteralPath $destination).Hash) {
            Copy-Item -LiteralPath $source -Destination $destination -Force
        }
    }
}
Write-Host "Visual Studio solution: $(Join-Path $build 'Gopher64UWP.slnx')"
Write-Host "Package: $package"
