#Requires -Version 7.6.5
[CmdletBinding()]
param([ValidateSet('Model','Full')][string]$Profile='Full',
    [ValidateSet('Debug','Release')][string]$Configuration='Debug',
    [string]$RepositoryRoot=(Split-Path -Parent $PSScriptRoot),[string]$WorkRoot='',[string]$ProductRoot='',[switch]$RequireGraphics)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'windows.ps1')
if (!$WorkRoot) { $WorkRoot=Join-Path $RepositoryRoot 'out' }
if (!$ProductRoot) { $ProductRoot=Join-Path $RepositoryRoot 'x64' }
$toolchain=Get-Elf3DToolchain
$parts=if($Profile -eq 'Model'){@('model')}else{@('model','application')}
$savedPath=$env:PATH
try {
    $env:PATH=$ProductRoot+';'+$env:PATH
    foreach ($part in $parts) {
        $name="elf3d_external_$part"
        $project=Join-Path $RepositoryRoot "examples/external_$part/$name.vcxproj"
        & $toolchain.MSBuild $project /m:4 /nologo /v:minimal "/p:Configuration=$Configuration" /p:Platform=x64 /p:VisualStudioVersion=18.0 `
            "/p:Elf3DSourceRoot=$RepositoryRoot\" "/p:Elf3DWorkRoot=$WorkRoot\" "/p:Elf3DProductRoot=$ProductRoot\"
        if ($LASTEXITCODE) { throw "External $part build failed." }
        $suffix=if($Configuration -eq 'Debug'){'D'}else{''}
        $executable=Join-Path $WorkRoot "tests/$Configuration/$name$suffix.exe"
        $runRoot=Join-Path $WorkRoot "tests/$Configuration/run"
        New-Item -ItemType Directory -Path $runRoot -Force | Out-Null
        $arguments=if ($part -eq 'model') { @((Join-Path $RepositoryRoot 'tests/fixtures/elf3d_smoke/elf3d_smoke.gltf'),(Join-Path $runRoot 'external-model.glb')) } else { @() }
        $result=Invoke-Elf3DProcess -File $executable -Arguments $arguments -WorkingDirectory $runRoot -Timeout 120
        if ($result.ExitCode -eq 77 -and !$RequireGraphics) { Write-Host "SKIPPED external ${part}: graphics unavailable" }
        elseif ($result.ExitCode -ne 0) { throw "External $part failed: $($result.Output)$($result.Error)" }
        else { Write-Host "PASS external $part [$Configuration]" }
    }
} finally { $env:PATH=$savedPath }
