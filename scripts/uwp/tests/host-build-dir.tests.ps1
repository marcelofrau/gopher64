$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../common.ps1')
$base = Join-Path ([IO.Path]::GetTempPath()) ("g64-host-dir-" + [guid]::NewGuid().ToString('N'))
$root = Join-Path $base 'repo'
$target = Join-Path $base 'physical-out'
$out = Join-Path $root 'out'
try {
    New-Item -ItemType Directory -Path $root, $target | Out-Null
    $normal = Join-Path $out 'uwp-host-vs2026'
    if ((Get-UwpHostBuildDir $root '2026') -ne $normal) { throw 'Missing out changed the normal path.' }
    New-Item -ItemType Directory -Path $out | Out-Null
    if ((Get-UwpHostBuildDir $root '2026') -ne $normal) { throw 'Ordinary out changed the normal path.' }
    [IO.Directory]::Delete($out)
    New-Item -ItemType Junction -Path $out -Target $target | Out-Null
    $expected = Join-Path $target 'uwp-host-vs2026-direct'
    if ((Get-UwpHostBuildDir $root '2026') -ne $expected) { throw 'Junction out was not resolved.' }
    Write-Host 'PASS: normal and junction host build paths.'
} finally {
    if (Test-Path -LiteralPath $out) { [IO.Directory]::Delete($out) }
    if (Test-Path -LiteralPath $target) { [IO.Directory]::Delete($target) }
    if (Test-Path -LiteralPath $root) { [IO.Directory]::Delete($root) }
    if (Test-Path -LiteralPath $base) { [IO.Directory]::Delete($base) }
}
