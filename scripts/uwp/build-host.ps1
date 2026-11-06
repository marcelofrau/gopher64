[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [switch]$SkipRust,
    [string]$DozenDir = $env:GOPHER64_DOZEN_DIR,
    [switch]$SkipDependencies,
    [switch]$InitSubmodules,
    [string]$PackageVersion
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'common.ps1')
$root = Resolve-UwpPhysicalPath (Join-Path $PSScriptRoot '../..')
$vs = Get-UwpVisualStudio
$output = Get-UwpHostOutputDir $root $vs.Version
$DozenDir = Get-UwpDozenDir $root $output $DozenDir
if (-not $DozenDir) { throw 'Provide out/uwp-dozen/vulkan_dzn.dll and dxil.dll, or set GOPHER64_DOZEN_DIR.' }
if ($InitSubmodules) {
    & git -C $root submodule update --init --recursive
    if ($LASTEXITCODE) { throw 'Submodule initialization failed.' }
}
if (-not $SkipDependencies) {
    & (Join-Path $PSScriptRoot 'build-deps.ps1') -Configuration $Configuration
}
$properties = @('/p:Platform=x64', "/p:Configuration=$Configuration", "/p:Gopher64SkipRust=$($SkipRust.IsPresent.ToString().ToLowerInvariant())",
    "/p:Gopher64DozenDir=$([IO.Path]::GetFullPath($DozenDir))")
if ($PackageVersion) { $properties += "/p:Gopher64PackageVersion=$PackageVersion" }
& (Join-Path $vs.Install 'MSBuild/Current/Bin/MSBuild.exe') (Join-Path $root 'Gopher64UWP.slnx') /t:Build /m /nologo @properties
if ($LASTEXITCODE) { throw "UWP host build failed ($LASTEXITCODE)." }
$package = Get-ChildItem (Join-Path $output 'AppPackages/*_Test/*.msix') |
    Where-Object { ($_.Name -like '*_Debug.msix') -eq ($Configuration -eq 'Debug') } |
    Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
if (-not $package) { throw 'Build did not produce the expected UWP package.' }
& (Join-Path $PSScriptRoot 'audit-imports.ps1') -Package $package.FullName
Write-Host "Visual Studio solution: $(Join-Path $root 'Gopher64UWP.slnx')"
Write-Host "Package: $($package.FullName)"
