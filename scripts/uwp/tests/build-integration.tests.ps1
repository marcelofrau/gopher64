param([string]$BuildDir = (Join-Path $PSScriptRoot '../../../out/uwp-host-vs2026'))

$ErrorActionPreference = 'Stop'
$hostProject = Get-Content -LiteralPath (Join-Path $BuildDir 'gopher64-uwp.vcxproj') -Raw
$rustProject = Get-Content -LiteralPath (Join-Path $BuildDir 'gopher64-rust.vcxproj') -Raw

if ($hostProject -notmatch '<ProjectReference Include="[^"]*gopher64-rust\.vcxproj"') {
    throw 'The host project does not build the Rust project first.'
}
foreach ($project in @($hostProject, $rustProject)) {
    if ($project -notmatch '<DisableFastUpToDateCheck>true</DisableFastUpToDateCheck>') {
        throw 'A project is missing the Visual Studio up-to-date check override.'
    }
}
foreach ($configuration in @('Debug', 'Release')) {
    if ($rustProject -notmatch "build\.ps1[^\r\n]*-Configuration $configuration -SkipDependencies") {
        throw "The Rust project is missing the $configuration build command."
    }
}
Write-Host 'PASS: generated VS host depends on matching Debug and Release Rust builds.'
