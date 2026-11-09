$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../common.ps1')
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$vs = Get-UwpVisualStudio
$dev = Join-Path $vs.Install 'Common7/Tools/VsDevCmd.bat'
$base = Join-Path ([IO.Path]::GetTempPath()) ('g64-host-log-' + [guid]::NewGuid().ToString('N'))
try {
    New-Item -ItemType Directory -Path $base | Out-Null
    $source = Join-Path $root 'uwp/host/host_log.cpp'
    $test = Join-Path $root 'uwp/host/tests/host_log_test.cpp'
    $exe = Join-Path $base 'test.exe'
    $logs = Join-Path $base 'logs'
    New-Item -ItemType Directory -Path $logs | Out-Null
    # 1. Every API used must be available to UWP apps.
    $app = "call `"$dev`" -arch=x64 -host_arch=x64 >nul && cl /nologo /c /EHsc /std:c++17 /DUNICODE /D_UNICODE /DWINAPI_FAMILY=WINAPI_FAMILY_APP /Fo`"$base\\app.obj`" `"$source`""
    & $env:ComSpec /d /c $app
    if ($LASTEXITCODE) { throw "host_log.cpp does not compile for WINAPI_FAMILY_APP: $LASTEXITCODE" }
    # 2. Behaviour on the PC (desktop CRT, same code).
    $cmd = "call `"$dev`" -arch=x64 -host_arch=x64 >nul && cl /nologo /EHsc /std:c++17 /DUNICODE /D_UNICODE /Fo`"$base\\`" /Fe`"$exe`" `"$source`" `"$test`" && `"$exe`" `"$logs`""
    & $env:ComSpec /d /c $cmd
    if ($LASTEXITCODE) { throw "host_log test failed: $LASTEXITCODE" }
    # 3. An unhandled fault must leave a [crash] line with the module and offset.
    $crashLogs = Join-Path $base 'crash'
    New-Item -ItemType Directory -Path $crashLogs | Out-Null
    & $exe $crashLogs crash
    if ($LASTEXITCODE -eq 0) { throw 'crash mode exited normally' }
    $log = Get-Content -Raw (Join-Path $crashLogs 'gopher64-host.log')
    if ($log -notmatch '\[crash\] unhandled exception 0xC0000005 at .*\(gopher64-uwp\.exe\+0x') {
        throw "crash line missing:`n$log"
    }
    Write-Host 'PASS: unhandled fault logged with module and offset.'
} finally {
    if (Test-Path -LiteralPath $base) { Remove-Item -LiteralPath $base -Recurse -Force }
}
