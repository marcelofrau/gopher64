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

$etw = @('    ADVAPI32.dll', '       18011C000 Import Address Table', '             10C EventRegister', '             10E EventUnregister', '             114 EventWriteTransfer', '    ole32.dll', '              51 CoGetMalloc')
if (-not (Test-UwpAllowedAdvapiImports $etw)) { throw 'Documented UWP ETW functions should be allowed.' }
if (Test-UwpAllowedAdvapiImports ($etw + @('    ADVAPI32.dll', '             123 RegOpenKeyExW'))) { throw 'Unexpected registry API must be rejected.' }
if (Test-UwpAllowedAdvapiImports @('    ADVAPI32.dll')) { throw 'Unparsed imports must be rejected.' }
if (Test-UwpAllowedAdvapiImports @('    ADVAPI32.dll', '              2A [NONAME]')) { throw 'Ordinal imports must be rejected.' }
if (Test-UwpAllowedAdvapiImports @('    ADVAPI32.dll', '             10C EventRegister', '             Ordinal 102')) { throw 'Mixed named and ordinal imports must be rejected.' }
Write-Host 'Advapi32 ETW allowlist checks passed.'
