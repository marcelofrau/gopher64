$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../common.ps1')
$store = 'C:\VS\VC\Tools\MSVC\1\lib\x64\store'
$desktop = 'C:\VS\VC\Tools\MSVC\1\lib\x64'
$sdk = 'C:\Windows Kits\10\lib\ucrt\x64'
if ((Get-UwpCargoLibraryPath "$store;$sdk") -ne "$desktop;$sdk") { throw 'Store CRT was not mapped for Cargo.' }
if ((Get-UwpCargoLibraryPath "$desktop;$sdk") -ne "$desktop;$sdk") { throw 'Desktop CRT path was changed.' }
if ((Get-UwpCargoLibraryPath "X:\store;$sdk") -ne "X:\store;$sdk") { throw 'Unrelated store directory was changed.' }
if ((Get-UwpCargoLibraryPath '') -ne '') { throw 'Empty LIB was changed.' }
Write-Host 'PASS: Cargo host CRT selection preserves unrelated library paths.'
