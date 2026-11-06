#Requires -Version 7.6.5
[CmdletBinding()]
param(
    [ValidateSet('Model','Full')][string]$Profile='Full',
    [ValidateSet('Debug','Release','Both')][string]$Configuration='Debug',
    [string]$Filter='.*', [switch]$Build, [switch]$Headers, [switch]$RequireGraphics,
    [string]$RepositoryRoot=(Split-Path -Parent $PSScriptRoot),
    [string]$WorkRoot='', [string]$ProductRoot='', [switch]$List
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'windows.ps1')
$RepositoryRoot=[IO.Path]::GetFullPath($RepositoryRoot)
if (!$WorkRoot) { $WorkRoot=Join-Path $RepositoryRoot 'out' }
if (!$ProductRoot) { $ProductRoot=Join-Path $RepositoryRoot 'x64' }
$WorkRoot=[IO.Path]::GetFullPath($WorkRoot)
$ProductRoot=[IO.Path]::GetFullPath($ProductRoot)
$configs=if ($Configuration -eq 'Both') { @('Debug','Release') } else { @($Configuration) }
$catalog=@(Get-Content -LiteralPath (Join-Path $RepositoryRoot 'tests/tests.json') -Raw | ConvertFrom-Json)
$results=[Collections.Generic.List[object]]::new()
$savedPath=$env:PATH
try {
    $env:PATH=$ProductRoot+';'+$env:PATH
    foreach ($config in $configs) {
        $selected=@($catalog | Where-Object { $Profile -in $_.profiles -and $config -in $_.configurations -and $_.name -match $Filter })
        if ($List) { $selected | Select-Object name,profiles,configurations; continue }
        if (!$selected.Count) { throw "No tests matched $Profile / $Filter" }
        $runRoot=Join-Path $WorkRoot "tests/$config/run"
        New-Item -ItemType Directory -Path $runRoot -Force | Out-Null
        # Reusing old fixtures is still recent use for Routine cleanup retention.
        [IO.Directory]::SetLastWriteTimeUtc($runRoot,[datetime]::UtcNow)
        $tokens=@{Root=$RepositoryRoot;ProductRoot=$ProductRoot;TestRoot=(Join-Path $WorkRoot "tests/$config");
            ToolRoot=(Join-Path $WorkRoot "tools/$config");RunRoot=$runRoot;Config=$config;Suffix=$(if($config -eq 'Debug'){'D'}else{''})}
        function Expand-TestValue([string]$value) {
            foreach ($key in $tokens.Keys) { $value=$value.Replace('{'+$key+'}',[string]$tokens[$key]) }
            return $value
        }
        if ($Build) {
            $buildArgs=@{Configuration=$config;RepositoryRoot=$RepositoryRoot;WorkRoot=$WorkRoot;ProductRoot=$ProductRoot}
            & (Join-Path $PSScriptRoot 'build.ps1') @buildArgs -Scope $(if($Profile -eq 'Model'){'Model'}else{'All'})
            & (Join-Path $PSScriptRoot 'build.ps1') @buildArgs -Scope Tests -ModelOnly:($Profile -eq 'Model')
            if ($Profile -eq 'Full') {
                & (Join-Path $PSScriptRoot 'build.ps1') @buildArgs -Scope Tools -Project elf3d_studio_environment_baker,elf3d_render_quality_capture,elf3d_render_benchmark
            }
        }
        foreach ($test in $selected) {
            $timer=[Diagnostics.Stopwatch]::StartNew()
            $status='passed'; $diagnostic=''
            try {
                $program=Expand-TestValue $test.program
                $arguments=@($test.arguments | ForEach-Object { Expand-TestValue $_ })
                if ($test.kind -eq 'fatal') {
                    foreach ($scenario in $test.scenarios) {
                        $result=Invoke-Elf3DProcess -File $program -Arguments @($arguments+$scenario) -WorkingDirectory $runRoot -Timeout 30
                        $diagnostic+=$result.Output+$result.Error
                        if ($result.ExitCode -eq $test.skipCode) { $status='skipped'; break }
                        $expected=if ($scenario -eq 'unexpected') {'Elf3D boundary encountered an unexpected exception'} else {'Elf3D memory allocation failed'}
                        if ($result.ExitCode -eq 0 -or !$result.Error.Contains($expected)) { throw "$scenario did not reach controlled fatal handling: $diagnostic" }
                    }
                } else {
                    $result=Invoke-Elf3DProcess -File $program -Arguments $arguments -WorkingDirectory $runRoot -Timeout $test.timeout
                    $diagnostic=$result.Output+$result.Error
                    if ($result.ExitCode -eq $test.skipCode) { $status='skipped' }
                    elseif ($result.ExitCode -ne 0) { throw "Exit $($result.ExitCode): $diagnostic" }
                }
                if ($status -eq 'skipped' -and $RequireGraphics) { throw "A required graphics test skipped: $($test.name)" }
            } catch { $status='failed'; $diagnostic=$_.Exception.Message }
            $results.Add([pscustomobject]@{Name=$test.name;Configuration=$config;Status=$status;Seconds=$timer.Elapsed.TotalSeconds;Diagnostic=$diagnostic})
            Write-Host "$($status.ToUpperInvariant()) $($test.name) [$config]"
            if ($status -eq 'failed') { Write-Host $diagnostic }
        }
        if ($Headers) {
            & (Join-Path $PSScriptRoot 'check-headers.ps1') -Profile $Profile -Configuration $config -RepositoryRoot $RepositoryRoot -WorkRoot $WorkRoot
        }
    }
} finally {
    $env:PATH=$savedPath
    if ($results.Count) {
        $reportRoot=Join-Path $WorkRoot 'reports/tests'
        New-Item -ItemType Directory -Path $reportRoot -Force | Out-Null
        $failed=@($results | Where-Object Status -eq 'failed')
        $report=Join-Path $reportRoot "$Profile-$Configuration-$(if($failed.Count){'failure'}else{'success'}).json"
        $results | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $report -Encoding utf8
    }
}
if (@($results | Where-Object Status -eq 'failed').Count) { throw 'Test suite failed. See out/reports/tests.' }
