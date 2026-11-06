#Requires -Version 7.6.5
[CmdletBinding()]
param(
    [ValidateSet('All','Model','Dependencies','Engine','ImGui','Viewer','Cmd','Tests','Tools')][string]$Scope='All',
    [ValidateSet('Debug','Release','Both')][string]$Configuration='Debug',
    [ValidateSet('Build','Rebuild','Clean')][string]$Target='Build',
    [string]$RepositoryRoot=(Split-Path -Parent $PSScriptRoot),
    [string]$WorkRoot='', [string]$ProductRoot='',
    [switch]$ModelOnly, [string[]]$Project=@(), [switch]$CheckOnly, [switch]$NoPch
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'windows.ps1')
$RepositoryRoot=[IO.Path]::GetFullPath($RepositoryRoot)
if (!$WorkRoot) { $WorkRoot=Join-Path $RepositoryRoot 'out' }
if (!$ProductRoot) { $ProductRoot=Join-Path $RepositoryRoot 'x64' }
$WorkRoot=[IO.Path]::GetFullPath($WorkRoot).TrimEnd('\','/')+'\'
$ProductRoot=[IO.Path]::GetFullPath($ProductRoot).TrimEnd('\','/')+'\'
$toolchain=Get-Elf3DToolchain
if ($CheckOnly) { $toolchain; return }
$projects=@(Get-Elf3DProjects $RepositoryRoot)
$Project=@($Project | ForEach-Object { $_.Split(',',[StringSplitOptions]::RemoveEmptyEntries) })
$parts=if ($Scope -eq 'All') { @('Dependencies','Engine','ImGui','Viewer','Cmd') } elseif ($Scope -eq 'Model') { @('Dependencies','Engine','Cmd') } else { @($Scope) }
$configurations=if ($Configuration -eq 'Both') { @('Debug','Release') } else { @($Configuration) }
if ($Target -eq 'Clean') { [array]::Reverse($parts) }
$revision=if (Get-Command git -ErrorAction SilentlyContinue) { & git -C $RepositoryRoot rev-parse --short HEAD 2>$null } else { '' }
if (!$revision) { $revision='exported-source' }
$logRoot=Join-Path $WorkRoot 'reports/build'
New-Item -ItemType Directory -Path $logRoot -Force | Out-Null
foreach ($config in $configurations) {
    foreach ($part in $parts) {
        $selected=@($projects | Where-Object { $_.Part -eq $part -and (!$Project.Count -or $_.Name -in $Project) -and
            (!(($Scope -eq 'Model') -or $ModelOnly) -or $_.Model) })
        if (!$selected.Count) { throw "No projects selected in $part. Check -Project and -ModelOnly." }
        # Solution references order each component. CPU-only and focused builds select projects explicitly.
        $entries=if ($Scope -ne 'Model' -and !$ModelOnly -and !$Project.Count) {
            @(Join-Path $RepositoryRoot "win/Elf3D-$part.slnx")
        } else { @($selected.Path) }
        foreach ($entry in $entries) {
            $name=[IO.Path]::GetFileNameWithoutExtension($entry)
            $log=Join-Path $logRoot "$name-$config-running.log"
            $arguments=@($entry,'/m:4',"/t:$Target","/p:Configuration=$config",'/p:Platform=x64',
                '/p:VisualStudioVersion=18.0','/p:PreferredToolArchitecture=x64',
                "/p:VCToolsVersion=$($toolchain.VCToolsVersion)","/p:Elf3DRevision=$revision",
                "/p:Elf3DWorkRoot=$WorkRoot", "/p:Elf3DProductRoot=$ProductRoot",'/nologo','/v:minimal',
                '/fl',"/flp:logfile=$log;verbosity=normal;encoding=UTF-8")
            if ($NoPch) { $arguments+='/p:Elf3DUsePch=false' }
            Write-Host "$Target $name [$config|x64]"
            & $toolchain.MSBuild @arguments
            $exitCode=$LASTEXITCODE
            $savedLog=Join-Path $logRoot "$name-$config-$(if($exitCode){'failure'}else{'success'}).log"
            if (Test-Path -LiteralPath $log) { Move-Item -LiteralPath $log -Destination $savedLog -Force }
            if ($exitCode -ne 0) { throw "MSBuild failed: $name [$config]. See $savedLog" }
        }
    }
    if ($Target -ne 'Clean' -and $Scope -in 'All','Model') {
        & (Join-Path $PSScriptRoot 'write-build-record.ps1') -RepositoryRoot $RepositoryRoot -WorkRoot $WorkRoot `
            -ProductRoot $ProductRoot -Configuration $config -Profile $(if($Scope -eq 'All'){'Full'}else{'Model'}) -Compiler $toolchain.VCToolsVersion
    }
}
