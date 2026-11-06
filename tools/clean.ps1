#Requires -Version 7.6.5
[CmdletBinding(SupportsShouldProcess)]
param(
    [ValidateSet('Routine','Temp','Tests','Build','All','Releases')][string]$Scope='Routine',
    [ValidateSet('Debug','Release','Both')][string]$Configuration='Both',
    [ValidateRange(1,1000)][int]$Keep=1,
    [ValidateRange(1,3650)][int]$OlderThanDays=7,
    [string]$RepositoryRoot=(Split-Path -Parent $PSScriptRoot)
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'storage.ps1')
$root=[IO.Path]::GetFullPath($RepositoryRoot).TrimEnd('\','/')
$out=[IO.Path]::GetFullPath((Join-Path $root 'out'))
$candidates=[Collections.Generic.List[string]]::new()
if ($Scope -in 'Routine','Temp','All') {
    foreach ($name in @('tmp','temp','public','validation/native','validation/public')) { $candidates.Add((Join-Path $out $name)) }
    foreach ($releasePath in @('release','release/validation')) {
        $releaseRoot=Join-Path $out $releasePath
        $null=Resolve-Elf3DGeneratedPath -AllowedRoot $out -LiteralPath $releaseRoot
        $candidates.Add((Join-Path $releaseRoot 'package-check'))
        if (Test-Path -LiteralPath $releaseRoot -PathType Container) {
            # Only the packager's exact staging name with a corresponding ZIP.
            foreach ($stage in Get-ChildItem -LiteralPath $releaseRoot -Directory -Force) {
                if ($stage.Name -match '^elf3d-viewer-\d+\.\d+\.\d+-windows-x64$' -and
                    (Test-Path -LiteralPath ($stage.FullName+'.zip') -PathType Leaf)) { $candidates.Add($stage.FullName) }
            }
        }
    }
}
$configs=if($Configuration -eq 'Both'){@('Debug','Release')}else{@($Configuration)}
foreach ($config in $configs) {
    if ($Scope -eq 'Routine') { $candidates.Add((Join-Path $out "tests/$config/run")) }
    if ($Scope -in 'Tests','All') {
        $candidates.Add((Join-Path $out "tests/$config"))
        $candidates.Add((Join-Path $out "tools/$config"))
    }
    if ($Scope -in 'Build','All') { $candidates.Add((Join-Path $out "obj/$config")) }
}
if ($Scope -eq 'Releases') {
    $releaseRoot=Join-Path $out 'release'
    $null=Resolve-Elf3DGeneratedPath -AllowedRoot $out -LiteralPath $releaseRoot
    if(Test-Path -LiteralPath $releaseRoot) {
        $archives=@(Get-ChildItem -LiteralPath $releaseRoot -File -Filter '*.zip' | Sort-Object LastWriteTimeUtc -Descending)
        foreach ($archive in @($archives | Select-Object -Skip $Keep)) { $candidates.Add($archive.FullName) }
    }
}
$plan=[Collections.Generic.List[object]]::new()
$cutoff=[datetime]::UtcNow.AddDays(-$OlderThanDays)
# Validate the entire plan before the first removal, including in -WhatIf.
foreach ($candidate in $candidates | Select-Object -Unique) {
    $data=Get-Elf3DGeneratedData -AllowedRoot $out -LiteralPath $candidate
    if ($data -and ($Scope -ne 'Routine' -or $data.NewestWriteUtc -lt $cutoff)) { $plan.Add($data) }
}
$selectedBytes=0L; $removedBytes=0L
foreach ($data in $plan) {
    $selectedBytes+=$data.Bytes
    if ($PSCmdlet.ShouldProcess($data.Path,('Remove generated data ({0:N2} MiB)' -f ($data.Bytes/1MB)))) {
        $removed=Remove-Elf3DGeneratedData -AllowedRoot $out -LiteralPath $data.Path -Confirm:$false
        if ($removed) { $removedBytes+=$removed.Bytes }
    }
}
Write-Host ('Cleanup {0}: {1} paths, {2:N2} MiB selected, {3:N2} MiB removed.' -f $Scope,$plan.Count,($selectedBytes/1MB),($removedBytes/1MB))
