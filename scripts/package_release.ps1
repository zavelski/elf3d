[CmdletBinding()]
param(
    [string]$Version = "0.1.0",
    [ValidateSet("Release")]
    [string]$Configuration = "Release",
    [string]$BuildDir = "out/build/windows-release",
    [string]$OutputDir = "out/release"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))

function Get-FullPathFromRepo {
    param([string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $repoRoot $Path))
}

function Assert-UnderRoot {
    param(
        [string]$Child,
        [string]$Root
    )

    $childFull = [System.IO.Path]::GetFullPath($Child)
    $rootFull = [System.IO.Path]::GetFullPath($Root).TrimEnd('\', '/')
    if (-not $childFull.StartsWith($rootFull, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Path '$childFull' is outside '$rootFull'."
    }
}

$buildRoot = Get-FullPathFromRepo $BuildDir
$outputRoot = Get-FullPathFromRepo $OutputDir

if (-not (Test-Path -LiteralPath $buildRoot)) {
    throw "Build directory not found: $buildRoot"
}

$binDir = Join-Path $buildRoot "bin/$Configuration"
$viewerExe = Join-Path $binDir "elf3d_viewer.exe"
$engineDll = Join-Path $binDir "elf3d.dll"

$requiredFiles = @($viewerExe, $engineDll, (Join-Path $repoRoot "LICENSE"), (Join-Path $repoRoot "THIRD_PARTY.md"))
foreach ($file in $requiredFiles) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) {
        throw "Required release file not found: $file"
    }
}

New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null

$packageName = "elf3d-viewer-$Version-windows-x64"
$stageRoot = Join-Path $outputRoot $packageName
$zipPath = Join-Path $outputRoot "$packageName.zip"
$checksumsPath = Join-Path $outputRoot "SHA256SUMS.txt"

Assert-UnderRoot -Child $stageRoot -Root $outputRoot
Assert-UnderRoot -Child $zipPath -Root $outputRoot
Assert-UnderRoot -Child $checksumsPath -Root $outputRoot

if (Test-Path -LiteralPath $stageRoot) {
    Remove-Item -LiteralPath $stageRoot -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $stageRoot | Out-Null

Copy-Item -LiteralPath $viewerExe -Destination $stageRoot
Copy-Item -LiteralPath $engineDll -Destination $stageRoot
Copy-Item -LiteralPath (Join-Path $repoRoot "LICENSE") -Destination $stageRoot
Copy-Item -LiteralPath (Join-Path $repoRoot "THIRD_PARTY.md") -Destination $stageRoot
Copy-Item -LiteralPath (Join-Path $repoRoot "third_party/licenses") `
    -Destination (Join-Path $stageRoot "third_party_licenses") -Recurse

$readme = @"
Elf3D Viewer $Version for Windows x64

Run:
  elf3d_viewer.exe

You can open .gltf or .glb files from File > Open, pass a model path as the
first command-line argument, or drop a model file onto the viewer window.

Requirements:
  Windows x64
  OpenGL 4.1 core-profile graphics driver
  Microsoft Visual C++ Redistributable compatible with Visual Studio 2022

Elf3D original source code is licensed under the MIT License. See LICENSE.
Third-party components remain governed by their own notices. See THIRD_PARTY.md
and third_party_licenses/.

Known 0.1.0 limitations include opaque-only rendering, no animation, no skins,
no morph targets, no compression extensions, no KTX2, no stable C ABI, and no
validated Linux or macOS build.
"@
$readme | Set-Content -LiteralPath (Join-Path $stageRoot "README.txt") -Encoding UTF8

if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}

Compress-Archive -Path (Join-Path $stageRoot "*") -DestinationPath $zipPath -Force

$hash = Get-FileHash -LiteralPath $zipPath -Algorithm SHA256
$hashLine = "$($hash.Hash.ToLowerInvariant())  $([System.IO.Path]::GetFileName($zipPath))"
$hashLine | Set-Content -LiteralPath $checksumsPath -Encoding ASCII

Write-Host "Created $zipPath"
Write-Host "Created $checksumsPath"
