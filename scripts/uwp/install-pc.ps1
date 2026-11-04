[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Package,
    [switch]$InstallDependencies,
    [switch]$Launch
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'common.ps1')
$pkg = Get-UwpPackage $Package
& (Join-Path $PSScriptRoot 'audit-imports.ps1') -Package $pkg.File.FullName
if ($InstallDependencies) {
    Add-AppxPackage -Path $pkg.File.FullName -DependencyPath $pkg.Dependencies.FullName
} else {
    Add-AppxPackage -Path $pkg.File.FullName
}
Write-Host "Installed $($pkg.File.Name). Existing LocalState is preserved."
if ($Launch) {
    $app = Get-AppxPackage -Name 'Gopher64.UWP.Dev'
    if (-not $app) { throw 'Installed app not found.' }
    Start-Process "shell:AppsFolder\$($app.PackageFamilyName)!App"
    Write-Host 'Launch requested; check the app and host log for the result.'
}
