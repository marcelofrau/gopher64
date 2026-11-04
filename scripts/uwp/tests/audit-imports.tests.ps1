$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../common.ps1')

foreach ($name in @('vccorlib140d_app.dll', 'msvcp140d_app.dll', 'vcruntime140d_app.dll',
                    'vcruntime140_1d_app.dll', 'ucrtbased.dll', 'VCRUNTIME140D.DLL')) {
    if (-not (Test-UwpDebugCrtImport $name)) { throw "Debug CRT import not detected: $name" }
}
foreach ($name in @('vccorlib140_app.dll', 'msvcp140_app.dll', 'vcruntime140_1_app.dll',
                    'ucrtbase.dll', 'api-ms-win-core-debug-l1-1-0.dll', 'SDL3.dll')) {
    if (Test-UwpDebugCrtImport $name) { throw "Valid import wrongly flagged: $name" }
}
Write-Host 'Debug CRT import checks passed.'
