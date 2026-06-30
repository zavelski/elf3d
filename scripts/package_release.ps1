[CmdletBinding()]
param(
    [string]$Version = "0.4.0",
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
$assetsDir = Join-Path $binDir "assets"

$requiredFiles = @($viewerExe, $engineDll, (Join-Path $repoRoot "LICENSE"), (Join-Path $repoRoot "THIRD_PARTY.md"))
foreach ($file in $requiredFiles) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) {
        throw "Required release file not found: $file"
    }
}
$requiredAssetDirs = @($assetsDir, (Join-Path $assetsDir "font"), (Join-Path $assetsDir "icon"))
foreach ($directory in $requiredAssetDirs) {
    if (-not (Test-Path -LiteralPath $directory -PathType Container)) {
        throw "Required release asset directory not found: $directory"
    }
}
$requiredAssets = @(
    (Join-Path $assetsDir "font/DroidSans.ttf"),
    (Join-Path $assetsDir "icon/open.png"),
    (Join-Path $assetsDir "icon/reload.png"),
    (Join-Path $assetsDir "icon/fit_view.png"),
    (Join-Path $assetsDir "icon/reset_camera.png"),
    (Join-Path $assetsDir "icon/select.png"),
    (Join-Path $assetsDir "icon/measure.png"),
    (Join-Path $assetsDir "icon/clipping_panel.png"),
    (Join-Path $assetsDir "icon/section_plane.png"),
    (Join-Path $assetsDir "icon/add_clipping_box.png"),
    (Join-Path $assetsDir "icon/clear_clipping.png"),
    (Join-Path $assetsDir "icon/hide_selected.png"),
    (Join-Path $assetsDir "icon/show_selected.png"),
    (Join-Path $assetsDir "icon/isolate_selected.png"),
    (Join-Path $assetsDir "icon/show_all.png"),
    (Join-Path $assetsDir "icon/reset_layout.png")
)
foreach ($asset in $requiredAssets) {
    if (-not (Test-Path -LiteralPath $asset -PathType Leaf)) {
        throw "Required release asset not found: $asset"
    }
}

$thirdPartyNotices = @(
    @{
        Source = Join-Path $repoRoot "third_party/cgltf/LICENSE"
        Name = "cgltf-LICENSE.txt"
    },
    @{
        Source = Join-Path $repoRoot "third_party/glad/LICENSE.txt"
        Name = "glad-LICENSE.txt"
    },
    @{
        Source = Join-Path $repoRoot "third_party/glfw/LICENSE.md"
        Name = "glfw-LICENSE.md"
    },
    @{
        Source = Join-Path $repoRoot "third_party/glm/copying.txt"
        Name = "glm-copying.txt"
    },
    @{
        Source = Join-Path $repoRoot "third_party/imgui/LICENSE.txt"
        Name = "imgui-LICENSE.txt"
    },
    @{
        Source = Join-Path $repoRoot "third_party/stb/LICENSE"
        Name = "stb-LICENSE.txt"
    },
    @{
        Source = Join-Path $repoRoot "apps/viewer/assets/font/DroidSans-LICENSE-APACHE-2.0.txt"
        Name = "droidsans-APACHE-2.0.txt"
    }
)
foreach ($notice in $thirdPartyNotices) {
    if (-not (Test-Path -LiteralPath $notice.Source -PathType Leaf)) {
        throw "Required third-party notice not found: $($notice.Source)"
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
Copy-Item -LiteralPath $assetsDir -Destination (Join-Path $stageRoot "assets") -Recurse
Copy-Item -LiteralPath (Join-Path $repoRoot "LICENSE") -Destination $stageRoot
Copy-Item -LiteralPath (Join-Path $repoRoot "THIRD_PARTY.md") -Destination $stageRoot

$thirdPartyNoticesDir = Join-Path $stageRoot "third_party_licenses"
New-Item -ItemType Directory -Force -Path $thirdPartyNoticesDir | Out-Null
foreach ($notice in $thirdPartyNotices) {
    Copy-Item -LiteralPath $notice.Source `
        -Destination (Join-Path $thirdPartyNoticesDir $notice.Name)
}

$readme = @"
Elf3D Viewer $Version for Windows x64

Run:
  elf3d_viewer.exe

You can open .gltf or .glb files from File > Open, pass a model path as the
first command-line argument, or drop a model file onto the viewer window.
The packaged assets directory contains the viewer font and toolbar icons and
must remain beside elf3d_viewer.exe.

Requirements:
  Windows x64
  OpenGL 4.1 core-profile graphics driver
  Microsoft Visual C++ Redistributable compatible with Visual Studio 2022

Elf3D original source code is licensed under the MIT License. See LICENSE.
Third-party components remain governed by their own notices. See THIRD_PARTY.md
and third_party_licenses/.

Known limitations include opaque-only rendering, no animation, no skins,
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
