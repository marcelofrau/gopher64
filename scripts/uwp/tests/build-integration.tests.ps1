param([string]$Root = (Join-Path $PSScriptRoot '../../..'), [switch]$CheckRecipes)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../common.ps1')
$root = Resolve-UwpPhysicalPath $Root
$project = Join-Path $root 'uwp/gopher64-uwp.vcxproj'
[xml]$xml = Get-Content $project -Raw
[xml]$solution = Get-Content (Join-Path $root 'Gopher64UWP.slnx') -Raw
if ($solution.Solution.Project.Path -ne 'uwp/gopher64-uwp.vcxproj' -or $solution.Solution.Project.DefaultStartup -ne 'true' -or @($solution.Solution.Project.Deploy).Count -ne 2) { throw 'The host must be the startup/deploy project for both configurations.' }
$ns = [Xml.XmlNamespaceManager]::new($xml.NameTable)
$ns.AddNamespace('m', 'http://schemas.microsoft.com/developer/msbuild/2003')
if (-not (Test-Path (Join-Path $root 'Gopher64UWP.slnx'))) { throw 'Missing tracked solution.' }
if ($xml.SelectSingleNode('//m:DisableFastUpToDateCheck', $ns).InnerText -ne 'true') { throw 'VS can skip Cargo.' }
$toolsets = $xml.SelectNodes('//m:PlatformToolset', $ns)
if ($toolsets[0].InnerText -ne 'v145' -or $toolsets[1].InnerText -ne 'v143' -or $toolsets[1].Condition -ne "'`$(VisualStudioVersion)' == '17.0'") { throw 'Toolset must match the selected VS version.' }
$target = $xml.SelectSingleNode('//m:Target[@Name="Gopher64PrepareHost"]', $ns)
if ($target.BeforeTargets -ne 'PrepareForBuild') { throw 'Rust must run before host compilation/link.' }
$commands = @($target.Exec | ForEach-Object { $_.Command })
if (-not ($commands -match 'build.ps1.*-Configuration.*\$\(Configuration\).*SkipDependencies')) { throw 'Missing configuration-matched Rust build.' }
$rust = $target.SelectSingleNode('m:Exec[contains(@Command, "build.ps1")]', $ns)
if ($rust.ContinueOnError -eq 'true') { throw 'Rust failure must block the host.' }
if ((Get-Content $project -Raw) -match '[A-Za-z]:[\\/](workspace|Builds|Users)[\\/]') { throw 'Machine-specific path in tracked project.' }
if ($CheckRecipes) {
    $output = Get-UwpHostOutputDir $root ''
    foreach ($configuration in @('Debug', 'Release')) {
        [xml]$recipe = Get-Content (Join-Path $output "$configuration/gopher64-uwp.build.appxrecipe") -Raw
        $layout = Join-Path $output "$configuration/AppX"
        if ($recipe.Project.PropertyGroup.LayoutDir -ne $layout) { throw "$configuration AppX layout does not match physical output." }
        $manifests = @($recipe.Project.ItemGroup | ForEach-Object { $_.AppXManifest } | Where-Object { $_ })
        if ($manifests[0].Include -ne (Join-Path $output "$configuration/AppxManifest.xml")) { throw "$configuration manifest traverses a junction." }
    }
}
Write-Host 'PASS: tracked solution, configuration-matched Cargo, fail-fast build and physical deployment paths.'
