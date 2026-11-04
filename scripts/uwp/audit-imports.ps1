[CmdletBinding()]
param([Parameter(Mandatory)][string]$Package)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'common.ps1')
$vs = Get-UwpVisualStudio
$tools = Join-Path $vs.Install 'VC/Tools/MSVC'
$toolset = Get-ChildItem -LiteralPath $tools -Directory | Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
if (-not $toolset) { throw "MSVC tools not found under $tools" }
$dumpbin = Join-Path $toolset.FullName 'bin/Hostx64/x64/dumpbin.exe'
if (-not (Test-Path -LiteralPath $dumpbin)) { throw "dumpbin not found: $dumpbin" }

$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$audit = Join-Path $repo ("out/uwp-host-vs$($vs.Version)/audit/" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $audit | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [IO.Compression.ZipFile]::OpenRead((Resolve-Path -LiteralPath $Package).Path)
try {
    foreach ($name in @('gopher64-uwp.exe', 'SDL3.dll', 'SDL3_ttf.dll')) {
        if (-not $zip.GetEntry($name)) { throw "Package is missing $name" }
    }
    $names = @('gopher64-uwp.exe') + @($zip.Entries | Where-Object { $_.FullName -match '^[^/\\]+\.dll$' } | ForEach-Object FullName)
    foreach ($name in $names) {
        $entry = $zip.GetEntry($name)
        [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, (Join-Path $audit $name))
    }
} finally { $zip.Dispose() }

$blocked = @('winmm.dll', 'user32.dll', 'gdi32.dll', 'shell32.dll', 'advapi32.dll', 'dwmapi.dll', 'uxtheme.dll',
             'vcruntime140.dll', 'vcruntime140_1.dll', 'msvcp140.dll', 'vccorlib140.dll')
$failures = @()
foreach ($name in $names) {
    $result = & $dumpbin /imports (Join-Path $audit $name)
    if ($LASTEXITCODE -ne 0) { throw "dumpbin failed for $name (exit $LASTEXITCODE)" }
    $result | Set-Content -LiteralPath (Join-Path $audit "$name.imports.txt") -Encoding utf8
    $modules = @($result | ForEach-Object { if ($_ -match '^\s{4}([A-Za-z0-9_.-]+\.dll)\s*$') { $Matches[1].ToLowerInvariant() } })
    if ($modules.Count -eq 0) { throw "No import modules parsed for $name; inspect dumpbin output." }
    Write-Host "$name imports: $($modules -join ', ')"
    foreach ($module in $modules) {
        if ($module -eq 'advapi32.dll' -and (Test-UwpAllowedAdvapiImports $result)) {
            Write-Host "$name imports only documented UWP ETW functions from advapi32.dll"
            continue
        }
        if ($module -in $blocked -or (Test-UwpDebugCrtImport $module)) {
            $failures += "$name imports $module"
        }
    }
}
Write-Host "Detailed import tables: $audit"
if ($failures.Count) { throw "Unsupported or Debug CRT module(s) found: $($failures -join '; '). Rebuild SDL dependencies in Release." }
Write-Warning 'Module check passed, but API-level Store certification, dynamic loads, and availability on Xbox still require validation.'
