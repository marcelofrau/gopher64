$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../common.ps1')
$base = Join-Path ([IO.Path]::GetTempPath()) ("g64-paths-" + [guid]::NewGuid().ToString('N'))
$root = Join-Path $base 'repo'
$out = Join-Path $root 'out'
$target = Join-Path $base 'storage'
$alias = Join-Path $base 'checkout-alias'
$appx = $null
try {
    New-Item -ItemType Directory -Path $root, $target -Force | Out-Null
    if ((Get-UwpHostBuildDir $root '2026') -ne $root) { throw 'Solution is not at checkout root.' }
    if ((Get-UwpHostOutputDir $root '2026') -ne (Join-Path $out 'uwp-host')) { throw 'Normal clone requires a junction.' }
    New-Item -ItemType Junction -Path $out -Target $target | Out-Null
    New-Item -ItemType Junction -Path $alias -Target $root | Out-Null
    if ((Get-UwpHostBuildDir $alias '2026') -ne $root) { throw 'Checkout alias was not resolved.' }
    $output = Join-Path $target 'uwp-host'
    if ((Get-UwpHostOutputDir $alias '2026') -ne $output) { throw 'Nested junction was not resolved.' }
    $appx = Join-Path $output 'Release/AppX'
    New-Item -ItemType Directory -Path (Split-Path $appx) -Force | Out-Null
    New-Item -ItemType Junction -Path $appx -Target $target | Out-Null
    try { Get-UwpHostOutputDir $root '2026'; throw 'Unsafe AppX junction accepted.' }
    catch { if ($_.Exception.Message -notlike '*AppX output subdirectory*') { throw } }
    Write-Host 'PASS: root solution, normal clone, nested output junction and unsafe AppX junction.'
} finally {
    foreach ($link in @($appx, $alias, $out)) {
        if ($link -and (Test-Path -LiteralPath $link)) { [IO.Directory]::Delete($link) }
    }
    if (Test-Path $base) { Remove-Item -LiteralPath $base -Recurse -Force }
}
