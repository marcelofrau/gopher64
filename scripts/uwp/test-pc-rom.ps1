[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Rom,
    [ValidateSet('Init', 'Frame')][string]$ExpectedStage = 'Init',
    [ValidateRange(1, 600)][int]$TimeoutSeconds = 20
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'common.ps1')
$package = Get-AppxPackage -Name 'Gopher64.UWP.Dev'
if (-not $package) { throw 'Install the development MSIX before running this test.' }
if (Get-Process 'gopher64-uwp' -ErrorAction SilentlyContinue) { throw 'Close the running Gopher64 UWP app first.' }
$source = (Resolve-Path -LiteralPath $Rom).Path
$local = Join-Path $env:LOCALAPPDATA "Packages\$($package.PackageFamilyName)\LocalState"
$autoboot = Join-Path $local 'autoboot.txt'
$probe = Join-Path $local 'p2-probe.txt'
$testMarker = Join-Path $local 'p2-rom-test.txt'
if ((Test-Path -LiteralPath $autoboot) -or (Test-Path -LiteralPath $probe) -or (Test-Path -LiteralPath $testMarker)) { throw 'Existing autoboot/probe configuration was not changed.' }
$copy = Join-Path $local ("p2-test-{0}{1}" -f [Guid]::NewGuid().ToString('N'), [IO.Path]::GetExtension($source))
$log = Join-Path $local 'gopher64-host.log'
$previous = @(Read-UwpLog $log).Count
$createdAutoboot = $false
$createdMarker = $false
try {
    [IO.File]::Copy($source, $copy)
    $stream = [IO.File]::Open($autoboot, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    $createdAutoboot = $true
    $writer = [IO.StreamWriter]::new($stream, [Text.UTF8Encoding]::new($false))
    try { $writer.WriteLine($copy) } finally { $writer.Dispose() }
    $stream = [IO.File]::Open($testMarker, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    $createdMarker = $true
    $stream.Dispose()
    Write-Host 'Starting the installed UWP app with a temporary copy of the ROM; gameplay is not a test criterion.'
    Start-Process "shell:AppsFolder\$($package.PackageFamilyName)!App"
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $successPattern = if ($ExpectedStage -eq 'Frame') { 'Dozen: first touched swapchain frame completed end_frame' } else { '\[rust\] rdp_init succeeded' }
    do {
        Start-Sleep -Milliseconds 250
        $lines = @(Read-UwpLog $log | Select-Object -Skip $previous)
        $text = $lines -join "`n"
    } while ((Get-Date) -lt $deadline -and $text -notmatch "Host exiting|\[rust\] panic:|$successPattern")
    $lines | ForEach-Object { Write-Host $_ }
    if ($text -match 'Rust returned|\[rust\] panic:') {
        Get-Process 'gopher64-uwp' -ErrorAction SilentlyContinue | Wait-Process -Timeout 5 -ErrorAction SilentlyContinue
    }
    if ($text -match '\[rust\] panic:') { throw 'Rust panicked during ROM startup; see the log above.' }
    if ($text -match 'Rust returned [1-9]' -and $text -notmatch 'Restored status frame presented') { throw 'The frontend did not display the error after Rust returned.' }
    if ($text -notmatch $successPattern) { throw "Did not reach the requested $ExpectedStage milestone; see the log above." }
    if ($text -match 'Rust returned [1-9]') { throw 'The Rust entry returned an error after reaching rdp_init; see the log above.' }
    if ($text -notmatch 'Host exiting' -and -not (Get-Process 'gopher64-uwp' -ErrorAction SilentlyContinue)) { throw 'The app exited unexpectedly; see the log above.' }
    Write-Host "Observed $ExpectedStage milestone; this does not prove image correctness, gameplay, or Xbox support."
} finally {
    if ($createdAutoboot) { Remove-Item -LiteralPath $autoboot }
    if ($createdMarker) { Remove-Item -LiteralPath $testMarker }
    if (Test-Path -LiteralPath $copy) {
        if (Get-Process 'gopher64-uwp' -ErrorAction SilentlyContinue) {
            Write-Warning "App is still running; temporary ROM retained at $copy. Close the app before deleting it."
        } else {
            Remove-Item -LiteralPath $copy
        }
    }
}
