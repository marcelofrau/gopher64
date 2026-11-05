$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../common.ps1')
$base = Join-Path ([IO.Path]::GetTempPath()) ("g64-dozen-" + [guid]::NewGuid().ToString('N'))
$root = Join-Path $base 'repo'
$build = Join-Path $root 'out/uwp-host-vs2026'
$preferred = Join-Path $root 'out/uwp-dozen'
$legacy = Join-Path $base 'scummvm-uwp/extern/retroarch/pkg/msvc-uwp/RetroArch-msvcUWP/MESA/x64'
try {
    New-Item -ItemType Directory -Path $build -Force | Out-Null
    if (Get-UwpDozenDir $root $build '') { throw 'Unexpected selection without inputs.' }
    New-Item -ItemType Directory -Path $legacy -Force | Out-Null
    try { Get-UwpDozenDir $root $build ''; throw 'Empty legacy pair was accepted.' }
    catch { if ($_.Exception.Message -notmatch 'missing vulkan_dzn.dll') { throw } }
    New-Item -ItemType File -Path (Join-Path $legacy 'vulkan_dzn.dll') | Out-Null
    try { Get-UwpDozenDir $root $build ''; throw 'Incomplete legacy pair was accepted.' }
    catch { if ($_.Exception.Message -notmatch 'missing dxil.dll') { throw } }
    New-Item -ItemType File -Path (Join-Path $legacy 'dxil.dll') | Out-Null
    if ((Resolve-Path (Get-UwpDozenDir $root $build '')).Path -ne (Resolve-Path $legacy).Path) { throw 'Legacy fallback not selected.' }
    New-Item -ItemType Directory -Path $preferred | Out-Null
    if ((Resolve-Path (Get-UwpDozenDir $root $build '')).Path -ne (Resolve-Path $legacy).Path) { throw 'Empty preferred directory blocked fallback.' }
    New-Item -ItemType File -Path (Join-Path $preferred 'vulkan_dzn.dll') | Out-Null
    if ((Resolve-Path (Get-UwpDozenDir $root $build '')).Path -ne (Resolve-Path $legacy).Path) { throw 'Partial pair blocked fallback.' }
    New-Item -ItemType File -Path (Join-Path $preferred 'dxil.dll') | Out-Null
    if ((Get-UwpDozenDir $root $build '') -ne $preferred) { throw 'Local pair not preferred.' }
    if ((Get-UwpDozenDir $root $build 'explicit') -ne 'explicit') { throw 'Explicit selection lost precedence.' }
    Write-Host 'PASS: explicit, local and legacy Dozen selection.'
} finally {
    if (Test-Path -LiteralPath $base) { Remove-Item -LiteralPath $base -Recurse -Force }
}
