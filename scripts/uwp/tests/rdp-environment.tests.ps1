$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../common.ps1')
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$vs = Get-UwpVisualStudio
$dev = Join-Path $vs.Install 'Common7/Tools/VsDevCmd.bat'
$base = Join-Path ([IO.Path]::GetTempPath()) ('g64-rdp-env-' + [guid]::NewGuid().ToString('N'))
try {
    New-Item -ItemType Directory -Path $base | Out-Null
    $test = Join-Path $root 'uwp/compat/tests/rdp_environment_test.cpp'
    $other = Join-Path $root 'uwp/compat/tests/rdp_environment_other.cpp'
    $exe = Join-Path $base 'test.exe'
    $cmd = "call `"$dev`" -arch=x64 -host_arch=x64 >nul && cl /nologo /EHsc /std:c++17 /Fo`"$base\\`" /Fe`"$exe`" `"$test`" `"$other`" && `"$exe`""
    & $env:ComSpec /d /c $cmd
    if ($LASTEXITCODE) { throw "RDP environment test failed: $LASTEXITCODE" }
    Write-Host 'PASS: RDP environment and cross-translation-unit subgroup policy.'
} finally {
    if (Test-Path -LiteralPath $base) { Remove-Item -LiteralPath $base -Recurse -Force }
}
