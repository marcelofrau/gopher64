param([string]$BuildDir)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../common.ps1')
$buildScript = Get-Content -Raw (Join-Path $PSScriptRoot '../../../build.rs')
if (-not $buildScript.Contains('cargo::rerun-if-changed=uwp/audio')) {
    throw 'Cargo must track UWP audio sources and headers.'
}
if (-not $BuildDir) {
    $root = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
    $BuildDir = Get-UwpHostBuildDir $root (Get-UwpVisualStudio).Version
}
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
