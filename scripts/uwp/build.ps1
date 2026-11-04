[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [switch]$SkipDependencies,
    [switch]$InitSubmodules
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$requiredFiles = @(
    'parallel-rdp/parallel-rdp-standalone/parallel-rdp/rdp_renderer.cpp',
    'retroachievements/rcheevos/src/rc_client.c'
)
$missing = @($requiredFiles | Where-Object { -not (Test-Path (Join-Path $repoRoot $_)) })
if ($missing.Count -and -not $InitSubmodules) {
    throw "Required submodules are missing. Re-run with -InitSubmodules (or run git submodule update --init --recursive). Missing: $($missing -join ', ')"
}
if ($missing.Count) {
    & git -C $repoRoot submodule update --init --recursive -- parallel-rdp/parallel-rdp-standalone retroachievements/rcheevos
    if ($LASTEXITCODE) { throw "Submodule initialization failed ($LASTEXITCODE)." }
    $missing = @($requiredFiles | Where-Object { -not (Test-Path (Join-Path $repoRoot $_)) })
    if ($missing.Count) { throw "Required submodule files are still missing: $($missing -join ', ')" }
}

if (-not (Get-Command rustup -ErrorAction SilentlyContinue)) { throw 'rustup not found on PATH.' }
if (-not (Get-Command clang-cl -ErrorAction SilentlyContinue)) { throw 'clang-cl not found on PATH; install LLVM and add clang-cl to PATH.' }
$toolchainFile = Get-Content (Join-Path $repoRoot 'rust-toolchain.toml') -Raw
$toolchainMatch = [regex]::Match($toolchainFile, '(?m)^\s*channel\s*=\s*"([^"]+)"')
if (-not $toolchainMatch.Success) { throw 'Could not read the pinned Rust channel from rust-toolchain.toml.' }
$toolchain = $toolchainMatch.Groups[1].Value
$components = & rustup component list --installed --toolchain $toolchain
if ($LASTEXITCODE -or -not ($components -match '^rust-src$')) {
    throw "rust-src is required for -Z build-std. Install it with: rustup component add rust-src --toolchain $toolchain"
}
$sysroot = (& rustup run $toolchain rustc --print sysroot).Trim()
$stdCargoLock = Join-Path $sysroot 'lib/rustlib/src/rust/library/Cargo.lock'
if (-not (Test-Path $stdCargoLock)) {
    throw "The pinned toolchain's rust-src is incomplete ($stdCargoLock missing). Reinstall it with: rustup component add rust-src --toolchain $toolchain"
}

$depLibs = @('lib/SDL3.lib', 'lib/SDL3_ttf.lib', 'bin/SDL3.dll', 'bin/SDL3_ttf.dll')
$missingDeps = @($depLibs | Where-Object { -not (Test-Path (Join-Path $repoRoot "out/uwp-deps/$_")) })
if ($SkipDependencies -and $missingDeps.Count) {
    throw "SDL dependencies are missing; omit -SkipDependencies to build them. Missing: $($missingDeps -join ', ')"
}
if (-not $SkipDependencies) {
    # The host uses the Store Release CRT in both configurations; SDL must match.
    & (Join-Path $PSScriptRoot 'build-deps.ps1') -Config Release
    if ($LASTEXITCODE) { throw "SDL dependency build failed ($LASTEXITCODE)." }
}

& (Join-Path $PSScriptRoot 'gen-links.ps1')
if ($LASTEXITCODE) { throw "Cargo links generation failed ($LASTEXITCODE)." }

$cargoProfile = if ($Configuration -eq 'Release') { 'release' } else { 'dev' }
$cargoArgs = @(
    'rustc', '--locked', '--lib',
    '--target', 'x86_64-uwp-windows-msvc',
    '--no-default-features',
    '-Z', 'build-std=std,panic_abort',
    '--config', (Join-Path $repoRoot '.cargo/uwp-links.toml'),
    '--config', "profile.$cargoProfile.panic='abort'"
)
if ($Configuration -eq 'Release') { $cargoArgs += '--release' }
$cargoArgs += @('--', '--crate-type', 'staticlib', '--print', 'native-static-libs')

$previousBootstrap = $env:RUSTC_BOOTSTRAP
Push-Location $repoRoot
try {
    $env:RUSTC_BOOTSTRAP = '1'
    & rustup run $toolchain cargo @cargoArgs
    if ($LASTEXITCODE) { throw "UWP staticlib build failed ($LASTEXITCODE)." }
} finally {
    $env:RUSTC_BOOTSTRAP = $previousBootstrap
    Pop-Location
}

$profile = $Configuration.ToLowerInvariant()
$artifact = Join-Path $repoRoot "target\x86_64-uwp-windows-msvc\$profile\deps\gopher64.lib"
if (-not (Test-Path $artifact)) { throw "Cargo succeeded but expected static library was not produced: $artifact" }
Write-Host "UWP static library: $artifact" -ForegroundColor Green
