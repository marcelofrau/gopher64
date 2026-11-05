[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [switch]$SkipDependencies,
    [switch]$InitSubmodules
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'common.ps1')
& (Join-Path $PSScriptRoot 'build-host.ps1') -Configuration $Configuration `
    -SkipDependencies:$SkipDependencies -InitSubmodules:$InitSubmodules
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$vs = Get-UwpVisualStudio
$build = Get-UwpHostBuildDir $root $vs.Version
$solution = Get-ChildItem $build -File | Where-Object { $_.Name -in @('Gopher64UWP.sln', 'Gopher64UWP.slnx') } | Select-Object -First 1
if (-not $solution) { throw "No Visual Studio solution was generated in $build." }
$devenv = Join-Path $vs.Install 'Common7/IDE/devenv.exe'
if (-not (Test-Path $devenv)) { throw "Visual Studio IDE not found: $devenv" }
Write-Host "Opening $($solution.FullName) in $devenv"
Start-Process $devenv -ArgumentList "`"$($solution.FullName)`""
