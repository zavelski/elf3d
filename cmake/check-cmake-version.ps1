[CmdletBinding()]
param(
    [string]$CMakeExecutable = "cmake",
    [string]$ExpectedVersion = "4.3.4"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
if (Get-Variable -Name PSNativeCommandUseErrorActionPreference -ErrorAction SilentlyContinue) {
    $PSNativeCommandUseErrorActionPreference = $false
}

$command = Get-Command $CMakeExecutable -ErrorAction SilentlyContinue
if ($null -eq $command) {
    throw "CMake executable '$CMakeExecutable' was not found."
}

$versionOutput = @(& $command.Source --version 2>&1)
if ($LASTEXITCODE -ne 0) {
    throw "CMake version query failed with exit code $LASTEXITCODE."
}

$match = [regex]::Match(($versionOutput -join "`n"), "cmake version ([0-9]+\.[0-9]+\.[0-9]+)")
if (-not $match.Success) {
    throw "Could not parse the CMake version from '$($versionOutput -join " ")'."
}

$actualVersion = $match.Groups[1].Value
if ($actualVersion -ne $ExpectedVersion) {
    throw "Expected CMake $ExpectedVersion, but '$($command.Source)' is CMake $actualVersion."
}

Write-Host "CI CMake version contract passed with $actualVersion."
