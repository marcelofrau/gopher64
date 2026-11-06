$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../common.ps1')
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$vs = Get-UwpVisualStudio
$msbuild = Join-Path $vs.Install 'MSBuild/Current/Bin/MSBuild.exe'
$base = Join-Path ([IO.Path]::GetTempPath()) ("g64-host-" + [guid]::NewGuid().ToString('N'))
$repo = Join-Path $base "checkout with space's"
try {
    New-Item -ItemType Directory -Path (Join-Path $repo 'uwp'), (Join-Path $repo 'scripts/uwp') -Force | Out-Null
    Copy-Item (Join-Path $root 'uwp/gopher64-uwp.vcxproj') (Join-Path $repo 'uwp')
    Set-Content (Join-Path $repo 'scripts/uwp/prepare-host.ps1') @'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$output = Join-Path $root 'out/uwp-host/Release'
$intermediate = Join-Path $root 'out/uwp-host/obj/Release'
New-Item -ItemType Directory -Path $intermediate -Force | Out-Null
Set-Content (Join-Path $intermediate 'physical-output.txt') $output
'@
    Set-Content (Join-Path $repo 'scripts/uwp/build.ps1') "Write-Error 'Expected Rust build failure'; exit 23"
    $log = & $msbuild (Join-Path $repo 'uwp/gopher64-uwp.vcxproj') /t:Build /p:Configuration=Release /p:Platform=x64 /nologo /v:minimal 2>&1
    if ($LASTEXITCODE -eq 0 -or ($log -join "`n") -notmatch 'Expected Rust build failure') {
        throw "The failed Rust build did not stop MSBuild: $log"
    }
    if (Test-Path (Join-Path $repo 'out/uwp-host/Release/gopher64-uwp.exe')) { throw 'Host was linked after a Rust failure.' }
    Write-Host 'PASS: Rust failure stops the native host, including checkout paths with spaces/apostrophes.'
} finally {
    if (Test-Path -LiteralPath $base) { Remove-Item -LiteralPath $base -Recurse -Force }
}
