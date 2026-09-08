#Requires -Version 7.6.5
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'windows-tools.ps1')
& (Join-Path $PSScriptRoot 'check-cmake-version.ps1')
$cmakeCommand = Get-Command cmake -CommandType Application | Select-Object -First 1
$suite = Resolve-Elf3DCMakeTools -CMakePath $cmakeCommand.Source
$installation = Get-Elf3DVisualStudio
$msbuild = Join-Path $installation 'MSBuild/Current/Bin/amd64/MSBuild.exe'
$msbuildVersion = @(& $msbuild -nologo -version) -join ''
if ($LASTEXITCODE -ne 0 -or [version]$msbuildVersion -lt [version]'18.0') {
    throw 'MSBuild 18 or newer is required.'
}
$toolVersion = (Get-Content -LiteralPath (Join-Path $installation 'VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt') -Raw).Trim()
$compiler = Join-Path $installation "VC/Tools/MSVC/$toolVersion/bin/Hostx64/x64/cl.exe"
$compilerVersion = (Get-Item -LiteralPath $compiler).VersionInfo.FileVersion
if ([version]$compilerVersion -lt [version]'19.50') { throw "Unsupported MSVC: $compilerVersion" }
Write-Host "Runner: $env:ImageOS $env:ImageVersion; PowerShell $($PSVersionTable.PSVersion)"
Write-Host "Visual Studio: $installation; MSBuild $msbuildVersion; MSVC $compilerVersion"
Write-Host "CMake/CTest/CPack $($suite.Version): $(Split-Path -Parent $suite.CMake)"
Write-Host 'The configured SDK and effective compiler are also verified by the generated-project contracts.'
