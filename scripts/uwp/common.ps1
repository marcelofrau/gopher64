function Test-UwpDebugCrtImport([string]$Module) {
    return $Module -match '^(vccorlib|msvcp|vcruntime|concrt|vcomp)[0-9_]*d(_app)?\.dll$|^ucrtbased\.dll$'
}

function Test-UwpAllowedAdvapiImports([string[]]$Lines) {
    $inside = $false
    $symbols = @()
    foreach ($line in $Lines) {
        if ($line -match '^\s{4}([A-Za-z0-9_.-]+\.dll)\s*$') {
            $inside = $Matches[1] -ieq 'advapi32.dll'
        } elseif ($line -match '^\s*Summary\s*$') {
            $inside = $false
        } elseif ($inside -and $line -match '^\s*Ordinal\s+\d+\s*$') {
            return $false
        } elseif ($inside -and $line -match '^\s+[0-9A-Fa-f]+\s+(\S+)\s*$') {
            $symbols += $Matches[1]
        }
    }
    if ($symbols.Count -eq 0) { return $false }
    foreach ($symbol in $symbols) {
        if ($symbol -cnotin @('EventRegister', 'EventUnregister', 'EventWriteTransfer')) { return $false }
    }
    return $true
}

function Resolve-UwpPhysicalPath([string]$Path, [int]$Depth = 0) {
    if ($Depth -gt 32) { throw "Too many links while resolving $Path" }
    if (-not [IO.Path]::IsPathRooted($Path)) { $Path = Join-Path (Get-Location).Path $Path }
    $path = [IO.Path]::GetFullPath($Path)
    $current = [IO.Path]::GetPathRoot($path)
    foreach ($part in $path.Substring($current.Length).Split([IO.Path]::DirectorySeparatorChar, [StringSplitOptions]::RemoveEmptyEntries)) {
        $next = Join-Path $current $part
        $item = Get-Item -LiteralPath $next -Force -ErrorAction SilentlyContinue
        if ($item -and $item.LinkType) {
            $target = $item.Target
            if (-not $target) { throw "Cannot resolve link: $next" }
            if (-not [IO.Path]::IsPathRooted($target)) { $target = Join-Path (Split-Path -Parent $next) $target }
            $current = Resolve-UwpPhysicalPath $target ($Depth + 1)
        } else {
            $current = $next
        }
    }
    return $current
}

function Get-UwpHostBuildDir([string]$Root, [string]$Version) {
    return Resolve-UwpPhysicalPath $Root
}

function Get-UwpHostOutputDir([string]$Root, [string]$Version) {
    $output = Resolve-UwpPhysicalPath (Join-Path $Root 'out/uwp-host')
    foreach ($part in @('Debug', 'Release', 'Debug/AppX', 'Release/AppX')) {
        $path = Join-Path $output $part
        if ((Resolve-UwpPhysicalPath $path) -ne $path) { throw "AppX output subdirectory must not be a junction: $path" }
    }
    return $output
}

function Assert-UwpRuntimeFiles($Zip) {
    foreach ($name in @('gopher64-uwp.exe', 'SDL3.dll', 'SDL3_ttf.dll', 'vulkan_dzn.dll', 'dxil.dll')) {
        if (-not $Zip.GetEntry($name)) { throw "Package is missing $name" }
    }
}

function Get-UwpDozenDir([string]$Root, [string]$BuildDir, [string]$Explicit) {
    if ($Explicit) { return $Explicit }
    $preferred = Join-Path (Split-Path -Parent $BuildDir) 'uwp-dozen'
    if (Test-Path -LiteralPath $preferred -PathType Container) {
        $files = @(@('vulkan_dzn.dll', 'dxil.dll') | Where-Object { -not (Test-Path -LiteralPath (Join-Path $preferred $_) -PathType Leaf) })
        if ($files.Count -eq 0) { return $preferred }
        Write-Warning "Local Dozen pair is incomplete: $($files -join ', '). Trying the legacy source."
    }
    $legacy = Join-Path $Root '../scummvm-uwp/extern/retroarch/pkg/msvc-uwp/RetroArch-msvcUWP/MESA/x64'
    if (Test-Path -LiteralPath $legacy -PathType Container) {
        foreach ($name in @('vulkan_dzn.dll', 'dxil.dll')) {
            if (-not (Test-Path -LiteralPath (Join-Path $legacy $name) -PathType Leaf)) {
                throw "Legacy Dozen directory is missing $name."
            }
        }
        return $legacy
    }
    return ''
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
