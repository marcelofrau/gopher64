$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../common.ps1')
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$rustBuild = Get-Content -Raw (Join-Path $root 'build.rs')
$project = Get-Content -Raw (Join-Path $root 'uwp/gopher64-uwp.vcxproj')
$cmake = Get-Content -Raw (Join-Path $root 'uwp/CMakeLists.txt')
foreach ($lib in @('d3d12', 'dxgi')) {
    if ($rustBuild -notmatch "cargo:rustc-link-lib=$lib" -or $project -notmatch "$lib\.lib" -or $cmake -notmatch "\b$lib\b") {
        throw "D3D12 diagnostic library $lib missing from a UWP build entry point."
    }
}
$vs = Get-UwpVisualStudio
$dev = Join-Path $vs.Install 'Common7/Tools/VsDevCmd.bat'
$include = Join-Path $root 'parallel-rdp/parallel-rdp-standalone/vulkan-headers/include'
$source = Join-Path $root 'uwp/compat/tests/d3d12_feature_log_test.cpp'
$base = Join-Path ([IO.Path]::GetTempPath()) ('g64-d3d12-feature-' + [guid]::NewGuid().ToString('N'))
try {
    New-Item -ItemType Directory -Path $base | Out-Null
    $obj = Join-Path $base 'test.obj'
    $exe = Join-Path $base 'test.exe'
    $cmd = "call `"$dev`" -arch=x64 -host_arch=x64 >nul && cl /nologo /EHsc /std:c++17 /I`"$include`" /Fo`"$obj`" /Fe`"$exe`" `"$source`" /link d3d12.lib dxgi.lib && `"$exe`""
    & $env:ComSpec /d /c $cmd
    if ($LASTEXITCODE -ne 0) { throw "D3D12 diagnostic test failed: $LASTEXITCODE" }
    Write-Host 'PASS: D3D12 diagnostic errors and direct adapter comparison.'
} finally {
    if (Test-Path -LiteralPath $base) { Remove-Item -LiteralPath $base -Recurse -Force }
}
