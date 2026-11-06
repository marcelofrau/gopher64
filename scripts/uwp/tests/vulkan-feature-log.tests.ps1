$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../common.ps1')
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$interface = Get-Content -Raw (Join-Path $root 'parallel-rdp/interface.cpp')
if ($interface -notmatch '(?s)g64_uwp_log_storage_features\(.{0,600}?storage_8bit_features\.sType,') {
    throw 'UWP diagnostic is not wired to Granite 8-bit feature state.'
}
if ($interface -notmatch '(?s)g64_uwp_log_storage_features\(.{0,600}?storage_8bit_features\.sType,\s*wsi->get_device\(\)\.get_device_features\(\)\.vk11_features\.storageBuffer16BitAccess,\s*wsi->get_device\(\)\.get_device_features\(\)\.vk12_features\.storageBuffer8BitAccess\)') {
    throw 'UWP diagnostic is not wired to Granite storage capabilities.'
}
$vs = Get-UwpVisualStudio
$dev = Join-Path $vs.Install 'Common7/Tools/VsDevCmd.bat'
$include = Join-Path $root 'parallel-rdp/parallel-rdp-standalone/vulkan-headers/include'
$source = Join-Path $root 'uwp/compat/tests/vulkan_feature_log_test.cpp'
$base = Join-Path ([IO.Path]::GetTempPath()) ('g64-vulkan-feature-' + [guid]::NewGuid().ToString('N'))
try {
    New-Item -ItemType Directory -Path $base | Out-Null
    $obj = Join-Path $base 'test.obj'
    $exe = Join-Path $base 'test.exe'
    $cmd = "call `"$dev`" -arch=x64 -host_arch=x64 >nul && cl /nologo /EHsc /std:c++17 /I`"$include`" /Fo`"$obj`" /Fe`"$exe`" `"$source`" && `"$exe`""
    & $env:ComSpec /d /c $cmd
    if ($LASTEXITCODE -ne 0) { throw "Vulkan feature diagnostic test failed: $LASTEXITCODE" }
    Write-Host 'PASS: raw and Granite Vulkan storage feature diagnostics.'
} finally {
    if (Test-Path -LiteralPath $base) { Remove-Item -LiteralPath $base -Recurse -Force }
}
