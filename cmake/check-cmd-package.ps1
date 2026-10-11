#Requires -Version 7.6.5
[CmdletBinding()]
param([Parameter(Mandatory)][string]$Archive, [Parameter(Mandatory)][string]$Verifier,
    [Parameter(Mandatory)][string]$SourceRoot, [Parameter(Mandatory)][string]$WorkRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $Verifier -PathType Leaf)) { throw 'Cmd package verifier is missing.' }
$root = [IO.Path]::GetFullPath((Join-Path $WorkRoot ([guid]::NewGuid().ToString('N'))))
New-Item -ItemType Directory -Force -Path $root | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::ExtractToDirectory([IO.Path]::GetFullPath($Archive), $root)
foreach ($file in @('elf3d_cmd.exe', 'elf3d_viewer.exe', 'elf3d.dll', 'docs/CMD.md', 'LICENSE', 'THIRD_PARTY.md')) {
    if (-not (Test-Path -LiteralPath (Join-Path $root $file) -PathType Leaf)) { throw "Package file missing: $file." }
}
Push-Location $root
try {
    & (Join-Path $SourceRoot 'apps/cmd/tests/round_trip.ps1') -Executable (Join-Path $root 'elf3d_cmd.exe') `
        -Verifier $Verifier -WorkRoot (Join-Path $root 'processing-check')
} finally { Pop-Location }
Write-Host "Extracted Cmd package verified: $root"
