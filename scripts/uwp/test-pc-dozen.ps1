[CmdletBinding()]
param(
    [ValidateSet('default', '8bit', 'surface')][string]$Mode = 'surface',
    [ValidateRange(1, 600)][int]$TimeoutSeconds = 30
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

if (Get-Process gopher64-uwp -ErrorAction SilentlyContinue) {
    throw 'Close the running Gopher64 app before starting the local Dozen probe.'
}
$package = Get-AppxPackage -Name 'Gopher64.UWP.Dev'
if (-not $package) { throw 'The Gopher64 UWP development package is not installed on this PC.' }
$local = Join-Path $env:LOCALAPPDATA "Packages/$($package.PackageFamilyName)/LocalState"
$marker = Join-Path $local 'p3-dozen-probe.txt'
$log = Join-Path $local 'gopher64-host.log'
if (Test-Path -LiteralPath $marker) { throw "Refusing to overwrite an existing marker: $marker" }
$before = @(Read-UwpLog $log).Count
$created = $false
try {
    $stream = [IO.File]::Open($marker, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    $created = $true
    $writer = [IO.StreamWriter]::new($stream, [Text.UTF8Encoding]::new($false))
    try { $writer.WriteLine($Mode) } finally { $writer.Dispose() }
    Write-Host "Starting the PC-only Dozen $Mode probe; no game will run."
    Start-Process "shell:AppsFolder\$($package.PackageFamilyName)!App"
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    do {
        Start-Sleep -Milliseconds 250
        $latest = @(Read-UwpLog $log | Select-Object -Skip $before)
    } while ((Get-Date) -lt $deadline -and -not ($latest -match 'Dozen probe finished with code '))
    $latest | ForEach-Object { Write-Host $_ }
    if (-not ($latest -match 'Dozen probe finished with code 0')) {
        throw 'Dozen probe failed or timed out; inspect the log and close any remaining app window.'
    }
    if ($Mode -eq 'surface' -and -not ($latest -match 'surface capabilities result=0')) {
        throw 'The probe did not report successful surface capabilities.'
    }
    Write-Host 'PASS: Dozen probe completed; this does not prove a rendered game or Xbox support.'
} finally {
    if ($created) { Remove-Item -LiteralPath $marker }
}
