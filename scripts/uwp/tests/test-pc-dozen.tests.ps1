$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Join-Path ([IO.Path]::GetTempPath()) ("g64-dozen-test-" + [Guid]::NewGuid().ToString('N'))
$oldLocalAppData = $env:LOCALAPPDATA
$env:LOCALAPPDATA = $root
$local = Join-Path $root 'Packages/g64.test/LocalState'
New-Item -ItemType Directory -Path $local -Force | Out-Null
[IO.File]::WriteAllText((Join-Path $root 'result.txt'), '0')

function Get-Process { param([string]$Name) }
function Get-AppxPackage { param([string]$Name) [pscustomobject]@{ PackageFamilyName = 'g64.test' } }
function Start-Process {
    param([string]$FilePath)
    $probeLog = Join-Path $env:LOCALAPPDATA 'Packages/g64.test/LocalState/gopher64-host.log'
    $probeResult = [IO.File]::ReadAllText((Join-Path $env:LOCALAPPDATA 'result.txt'))
    Add-Content -LiteralPath $probeLog -Value 'GPU[0] surface capabilities result=0'
    Add-Content -LiteralPath $probeLog -Value "Dozen probe finished with code $probeResult"
}

try {
    $scriptPath = Join-Path $PSScriptRoot '../test-pc-dozen.ps1'
    $marker = Join-Path $local 'p3-dozen-probe.txt'
    & $scriptPath -Mode surface -TimeoutSeconds 2
    if (Test-Path -LiteralPath $marker) { throw 'Success left the probe marker behind.' }

    [IO.File]::WriteAllText((Join-Path $root 'result.txt'), '1')
    $failed = $false
    try { & $scriptPath -Mode surface -TimeoutSeconds 2 } catch { $failed = $true }
    if (-not $failed -or (Test-Path -LiteralPath $marker)) { throw 'Failure was hidden or left its marker behind.' }

    Set-Content -LiteralPath $marker -Value 'user-owned'
    $refused = $false
    try { & $scriptPath -Mode surface -TimeoutSeconds 2 } catch { $refused = $true }
    if (-not $refused -or (Get-Content -LiteralPath $marker -Raw).Trim() -ne 'user-owned') {
        throw 'Existing marker was not preserved.'
    }
    Write-Host 'PASS: Dozen probe success, failure, and existing-marker checks.'
} finally {
    $env:LOCALAPPDATA = $oldLocalAppData
    Remove-Item -LiteralPath $root -Recurse -Force
}
