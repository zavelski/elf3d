#Requires -Version 7.6.5
[CmdletBinding()]
param([string]$BuildRoot = '')

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'windows-tools.ps1')
$repository = Split-Path -Parent $PSScriptRoot
if (-not $BuildRoot) { $BuildRoot = Join-Path $repository 'win' }
$root = [System.IO.Path]::GetFullPath($BuildRoot)
$tools = Resolve-Elf3DCMakeTools
$installation = Get-Elf3DVisualStudio -VersionRange '[18.0,19.0)'
Write-Host "Visual Studio: $installation"
Write-Host "CMake: $($tools.CMake) ($($tools.Version))"
foreach ($component in @('dependencies', 'engine', 'imgui', 'viewer')) {
    & $tools.CMake --preset "win-$component" -S $repository -B (Join-Path $root $component) `
        "-DELF3D_COMPONENT_ROOT=$root" "-DCMAKE_GENERATOR_INSTANCE:INTERNAL=$installation"
    if ($LASTEXITCODE -ne 0) { throw "Configuration failed: $component (exit $LASTEXITCODE)." }
}
Write-Host "`nBuild in order, with the same Debug|x64 or Release|x64 configuration:"
foreach ($name in @('Dependencies', 'Engine', 'ImGui', 'Viewer')) {
    Write-Host (Join-Path $root "$($name.ToLowerInvariant())/Elf3D-$name.slnx")
}
Write-Host "Viewer: $(Join-Path $root 'viewer/bin/Debug/elf3d_viewer.exe')"
Write-Host "Viewer: $(Join-Path $root 'viewer/bin/Release/elf3d_viewer.exe')"
