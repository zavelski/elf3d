#Requires -Version 7.6.5
[CmdletBinding()]
param([ValidateSet('Debug','Release')][string]$Configuration='Debug',
    [string]$RepositoryRoot=(Split-Path -Parent $PSScriptRoot),
    [string]$WorkRoot='',[string]$ProductRoot='')
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'windows.ps1')
if (!$WorkRoot) { $WorkRoot=Join-Path $RepositoryRoot 'out' }
if (!$ProductRoot) { $ProductRoot=Join-Path $RepositoryRoot 'x64' }
$arguments=@{RepositoryRoot=$RepositoryRoot;WorkRoot=$WorkRoot;ProductRoot=$ProductRoot;Configuration=$Configuration}
$suffix=if($Configuration -eq 'Debug'){'D'}else{''}
$owned=@("elf3d_viewer$suffix.exe","elf3d_app$suffix.lib","elf3d_imgui$suffix.lib")
function Get-BuildSnapshot([switch]$Upstream) {
    $snapshot=@{}
    $files=@(Get-ChildItem -LiteralPath $ProductRoot -File)
    foreach ($directory in @((Join-Path $WorkRoot "obj/$Configuration"),(Join-Path $WorkRoot "symbols/$Configuration"))) {
        if (Test-Path -LiteralPath $directory) { $files+=@(Get-ChildItem -LiteralPath $directory -File -Recurse) }
    }
    foreach ($file in $files) {
        if ($file.Extension -notin '.obj','.pch','.exe','.dll','.lib','.pdb') { continue }
        if ($Upstream -and ($file.Name -in $owned -or $file.FullName -match '[\\/]elf3d_(app|imgui|viewer)[\\/]' -or $file.Name -eq "elf3d_viewer$suffix.pdb")) { continue }
        $snapshot[$file.FullName]="$($file.Length):$($file.LastWriteTimeUtc.Ticks)"
    }
    return $snapshot
}
function Assert-Unchanged($Before,$After,[string]$Label) {
    if ($Before.Count -ne $After.Count) { throw "$Label changed the set of cached products/objects." }
    foreach ($key in $Before.Keys) {
        if (!$After.ContainsKey($key) -or $Before[$key] -ne $After[$key]) { throw "$Label changed $key" }
    }
}
$before=Get-BuildSnapshot
& (Join-Path $PSScriptRoot 'build.ps1') @arguments -Scope All
Assert-Unchanged $before (Get-BuildSnapshot) 'Repeated build'
$upstream=Get-BuildSnapshot -Upstream
& (Join-Path $PSScriptRoot 'build.ps1') @arguments -Scope Viewer -Target Clean
Assert-Unchanged $upstream (Get-BuildSnapshot -Upstream) 'Viewer Clean'
foreach ($name in $owned) { if (Test-Path -LiteralPath (Join-Path $ProductRoot $name)) { throw "Viewer Clean retained its output: $name" } }
& (Join-Path $PSScriptRoot 'build.ps1') @arguments -Scope Viewer -Target Rebuild
Assert-Unchanged $upstream (Get-BuildSnapshot -Upstream) 'Viewer Rebuild'
foreach ($name in $owned) { if (!(Test-Path -LiteralPath (Join-Path $ProductRoot $name))) { throw "Viewer Rebuild did not restore $name" } }
$unexpected=@(Get-ChildItem -LiteralPath $ProductRoot -File | Where-Object Extension -notin '.exe','.dll','.lib')
if ($unexpected.Count) { throw "Unexpected files in product directory: $($unexpected.Name -join ', ')" }
$version=(Get-Content -LiteralPath (Join-Path $RepositoryRoot 'VERSION') -Raw).Trim()
foreach ($name in @("elf3d$suffix.dll","elf3d_viewer$suffix.exe","elf3d_cmd$suffix.exe")) {
    $info=[Diagnostics.FileVersionInfo]::GetVersionInfo((Join-Path $ProductRoot $name))
    if ($info.ProductVersion -ne $version) { throw "Incorrect version resource: $name" }
}
& (Join-Path $PSScriptRoot 'write-build-record.ps1') @arguments -Profile Full -Compiler (Get-Elf3DToolchain).VCToolsVersion
Write-Host "PASS incremental build and independent Viewer Clean/Rebuild [$Configuration]."
