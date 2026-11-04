function Test-UwpDebugCrtImport([string]$Module) {
    return $Module -match '^(vccorlib|msvcp|vcruntime|concrt|vcomp)[0-9_]*d(_app)?\.dll$|^ucrtbased\.dll$'
}

function Get-UwpVisualStudio {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) { throw 'vswhere.exe not found; install Visual Studio with the UWP C++ tools.' }
    $cmakeHelp = (& cmake --help) -join "`n"
    foreach ($candidate in @(
        @{ Generator = 'Visual Studio 18 2026'; Version = '2026'; Range = '[18.0,19.0)' },
        @{ Generator = 'Visual Studio 17 2022'; Version = '2022'; Range = '[17.0,18.0)' }
    )) {
        if ($cmakeHelp -notmatch [regex]::Escape($candidate.Generator)) { continue }
        $install = & $vswhere -products * -version $candidate.Range `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 Microsoft.VisualStudio.ComponentGroup.UWP.VC `
            -property installationPath | Select-Object -First 1
        if ($install) {
            return [pscustomobject]@{ Generator = $candidate.Generator; Version = $candidate.Version; Install = $install }
        }
    }
    throw 'No supported Visual Studio install with the UWP C++ tools and CMake generator was found.'
}

function Read-UwpLog([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) { return }
    $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
    $reader = [IO.StreamReader]::new($stream)
    try { while ($null -ne ($line = $reader.ReadLine())) { $line } }
    finally { $reader.Dispose() }
}

function Get-UwpPackage([string]$Path) {
    $item = Get-Item -LiteralPath $Path
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [IO.Compression.ZipFile]::OpenRead($item.FullName)
    try {
        foreach ($name in 'AppxManifest.xml', 'gopher64-uwp.exe', 'SDL3.dll', 'SDL3_ttf.dll') {
            if (-not $zip.GetEntry($name)) { throw "Package missing $name" }
        }
        $reader = [IO.StreamReader]::new($zip.GetEntry('AppxManifest.xml').Open())
        try { [xml]$manifest = $reader.ReadToEnd() } finally { $reader.Dispose() }
        if ($manifest.Package.Identity.Name -ne 'Gopher64.UWP.Dev' -or $manifest.Package.Identity.ProcessorArchitecture -ne 'x64') {
            throw 'Package is not the Gopher64 x64 development app.'
        }
        $dependencyDir = Join-Path $item.DirectoryName 'Dependencies/x64'
        $dependencies = @(Get-ChildItem -LiteralPath $dependencyDir -Filter 'Microsoft.VCLibs*.appx' -File -ErrorAction SilentlyContinue)
        if ($dependencies.Count -eq 0) { throw "No x64 VCLibs dependency in $dependencyDir" }
        return [pscustomobject]@{ File = $item; Version = $manifest.Package.Identity.Version; Dependencies = $dependencies }
    } finally { $zip.Dispose() }
}
