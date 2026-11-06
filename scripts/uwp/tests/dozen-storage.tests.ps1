[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$DriverDirectory,
    [Parameter(Mandatory)][string]$Glslang
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot '../common.ps1')
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$driver = Join-Path (Resolve-Path $DriverDirectory).Path 'vulkan_dzn.dll'
$dxil = Join-Path (Split-Path -Parent $driver) 'dxil.dll'
$glslangExe = (Resolve-Path $Glslang).Path
if (-not (Test-Path -LiteralPath $driver -PathType Leaf)) { throw "Driver not found: $driver" }
if (-not (Test-Path -LiteralPath $dxil -PathType Leaf)) { throw "DXIL not found: $dxil" }
$vs = Get-UwpVisualStudio
$dev = Join-Path $vs.Install 'Common7/Tools/VsDevCmd.bat'
$vc = Get-AppxPackage -Name 'Microsoft.VCLibs.140.00' | Where-Object Architecture -eq 'X64' | Sort-Object Version -Descending | Select-Object -First 1
if (-not $vc) { throw 'Microsoft.VCLibs.140.00 x64 is required for this desktop probe.' }
$base = Join-Path ([IO.Path]::GetTempPath()) ('g64-storage-probe-' + [guid]::NewGuid().ToString('N'))
$names = @('PATH', 'VK_DRIVER_FILES', 'VK_ICD_FILENAMES', 'dzn_enable_8bit_loads_stores',
           'G64_TEST_DOZEN_PATH', 'G64_TEST_DXIL_PATH')
$previous = @{}
foreach ($name in $names) { $previous[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
try {
    New-Item -ItemType Directory -Path $base | Out-Null
    $volk = Join-Path $root 'parallel-rdp/parallel-rdp-standalone/volk'
    $headers = Join-Path $root 'parallel-rdp/parallel-rdp-standalone/vulkan-headers/include'
    $source = Join-Path $PSScriptRoot 'dozen-storage/storage_probe.cpp'
    $shader = Join-Path $PSScriptRoot 'dozen-storage/storage_test.comp'
    $volkSource = Join-Path $volk 'volk.c'
    $volkObject = Join-Path $base 'volk.obj'
    $object = Join-Path $base 'probe.obj'
    $exe = Join-Path $base 'probe.exe'
    $cmd = "call `"$dev`" -arch=x64 -host_arch=x64 >nul && cl /nologo /c /I`"$volk`" /I`"$headers`" /Fo`"$volkObject`" `"$volkSource`""
    $cmd += " && cl /nologo /EHsc /std:c++17 /I`"$volk`" /I`"$headers`" /Fo`"$object`" /Fe`"$exe`" `"$source`" `"$volkObject`""
    & $env:ComSpec /d /c $cmd
    if ($LASTEXITCODE) { throw "Probe compilation failed ($LASTEXITCODE)." }
    $icd = Join-Path $base 'icd.json'
    @{file_format_version='1.0.0'; ICD=@{library_path=$driver; api_version='1.2.0'}} |
        ConvertTo-Json | Set-Content -LiteralPath $icd -Encoding utf8
    $env:PATH = "$($vc.InstallLocation);$(Split-Path -Parent $driver);$env:PATH"
    $env:VK_DRIVER_FILES = $icd
    $env:VK_ICD_FILENAMES = $icd
    $env:dzn_enable_8bit_loads_stores = 'true'
    $env:G64_TEST_DOZEN_PATH = $driver
    $env:G64_TEST_DXIL_PATH = $dxil
    Write-Host "Dozen SHA-256: $((Get-FileHash $driver).Hash)"
    Write-Host "DXIL SHA-256: $((Get-FileHash $dxil).Hash)"
    $text = Get-Content -Raw -LiteralPath $shader
    if ([regex]::Matches($text, 'halves\[(i|peer)\]').Count -ne 2) { throw 'Unexpected shader layout.' }
    $alias = $text.Replace('halves[i]', 'halves[i + 32u]').Replace('halves[peer]', 'halves[peer + 32u]')
    $sources = @{
        normal = $text
        alias = $alias
        mutant8 = $text.Replace('i * 17u + 3u', 'i * 17u + 4u')
        mutant16 = $alias.Replace('i * 1009u + 7u', 'i * 1009u + 8u')
        mutantRead = $text.Replace('(i + 1u) & 63u', 'i')
    }
    if ($sources.mutant8 -eq $text -or $sources.mutant16 -eq $alias -or $sources.mutantRead -eq $text) {
        throw 'Negative-control mutation failed.'
    }
    foreach ($name in $sources.Keys) {
        $comp = Join-Path $base "$name.comp"
        $spv = Join-Path $base "$name.spv"
        [IO.File]::WriteAllText($comp, $sources[$name])
        & $glslangExe -V --target-env vulkan1.2 $comp -o $spv
        if ($LASTEXITCODE) { throw "Shader compilation failed: $name" }
    }
    foreach ($name in @('normal', 'alias')) {
        $probeArgs = @((Join-Path $base "$name.spv"))
        if ($name -eq 'alias') { $probeArgs += '--alias' }
        for ($i = 0; $i -lt 5; $i++) {
            $output = & $exe @probeArgs 2>&1 | Out-String
            if ($LASTEXITCODE) { throw "Probe $name failed: $output" }
            if ($i -eq 0) { Write-Host $output }
        }
    }
    foreach ($name in @('mutant8', 'mutant16', 'mutantRead')) {
        $probeArgs = @((Join-Path $base "$name.spv"))
        if ($name -eq 'mutant16') { $probeArgs += '--alias' }
        $output = & $exe @probeArgs 2>&1 | Out-String
        if ($LASTEXITCODE -eq 0 -or $output -notmatch '(?m)^mismatch \d+: byte=\d+ half=\d+ combined=\d+\r?$') {
            throw "Negative control $name did not fail as expected: $output"
        }
    }
    foreach ($name in @('G64_TEST_DOZEN_PATH', 'G64_TEST_DXIL_PATH')) {
        $correct = [Environment]::GetEnvironmentVariable($name, 'Process')
        try {
            [Environment]::SetEnvironmentVariable($name, "$correct.invalid", 'Process')
            $output = & $exe (Join-Path $base 'normal.spv') 2>&1 | Out-String
            if ($LASTEXITCODE -eq 0 -or $output -notmatch 'module mismatch') {
                throw "Incorrect module path was not detected: $name $output"
            }
        } finally {
            [Environment]::SetEnvironmentVariable($name, $correct, 'Process')
        }
    }
    Write-Host 'PASS: repeated storage tests, shader controls and module-path controls.'
} finally {
    foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name, $previous[$name], 'Process') }
    if (Test-Path -LiteralPath $base) { [IO.Directory]::Delete($base, $true) }
}
