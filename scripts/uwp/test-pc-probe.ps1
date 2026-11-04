[CmdletBinding()]
param([int]$TimeoutSeconds = 30)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'common.ps1')
$package = Get-AppxPackage -Name 'Gopher64.UWP.Dev' | Select-Object -First 1
if (-not $package) { throw 'The Gopher64.UWP.Dev package is not installed for this user.' }
if (Get-Process gopher64-uwp -ErrorAction SilentlyContinue) { throw 'Close the running Gopher64 UWP app before this test.' }
$local = Join-Path $env:LOCALAPPDATA "Packages/$($package.PackageFamilyName)/LocalState"
if (-not (Test-Path $local)) { New-Item -ItemType Directory -Path $local | Out-Null }
$autoboot = Join-Path $local 'autoboot.txt'
$marker = Join-Path $local 'p2-probe.txt'
$log = Join-Path $local 'gopher64-host.log'
if (Test-Path $autoboot) { throw "Refusing to start a configured autoboot during the probe: $autoboot" }
if (Test-Path $marker) { throw "Refusing to overwrite an existing probe marker: $marker" }
$previous = @(Read-UwpLog $log).Count
$stream = [System.IO.File]::Open($marker, [System.IO.FileMode]::CreateNew, [System.IO.FileAccess]::Write)
$stream.Dispose()
try {
    Start-Process "shell:AppsFolder\$($package.PackageFamilyName)!App"
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $new = @()
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 500
        $new = @(Read-UwpLog $log | Select-Object -Skip $previous)
        if ($new -match 'Host exiting') { break }
    }
    $text = $new -join "`n"
    Write-Host $text
    foreach ($expected in @('First status frame presented', '\[rust\] entry started',
                            '\[rust\] error running game:.*Could not read ROM file',
                              'Rust probe returned 1 \(expected 1: ROM not found\)', 'Host exiting')) {
        if ($text -notmatch $expected) { throw "Probe failed or timed out; missing log pattern: $expected" }
    }
    Write-Host 'PC UWP probe passed.'
} finally {
    Remove-Item -LiteralPath $marker
}
