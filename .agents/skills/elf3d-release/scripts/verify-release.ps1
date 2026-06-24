[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern("^[0-9]+\.[0-9]+\.[0-9]+$")]
    [string]$Version,

    [ValidateSet("Absent", "Present", "Either")]
    [string]$TagState = "Either",

    [switch]$RequireSynchronizedBranches,
    [switch]$RequireGitHubRelease,
    [switch]$SkipBuild,
    [switch]$SkipPackage
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\..\.."))
$tagName = "v$Version"

function Invoke-External {
    param(
        [Parameter(Mandatory = $true)][string]$Executable,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    $display = "$Executable $($Arguments -join ' ')"
    Write-Host "==> $display"
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $display"
    }
}

function Get-RequiredTool {
    param([Parameter(Mandatory = $true)][string]$Name)

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if (-not $command) {
        throw "Required tool '$Name' was not found on PATH."
    }
    return $command.Source
}

function Test-ExpectedRepository {
    $top = [System.IO.Path]::GetFullPath((& git rev-parse --show-toplevel).Trim())
    if ($top.TrimEnd('\', '/') -ne $repoRoot.TrimEnd('\', '/')) {
        throw "Script resolved repository root '$repoRoot' but git returned '$top'."
    }

    $origin = (& git config --get remote.origin.url).Trim()
    if ($origin -notmatch "^(https://github\.com/zavelski/elf3d(\.git)?|git@github\.com:zavelski/elf3d(\.git)?)$") {
        throw "Unexpected origin URL '$origin'. Expected zavelski/elf3d."
    }

    Invoke-External git @("fetch", "origin", "--prune", "--tags")
}

function Test-CleanWorkingTree {
    $status = & git status --porcelain
    if ($status) {
        throw "Working tree is not clean. Commit or remove changes before release verification."
    }
    Write-Host "Working tree is clean."
}

function Test-VersionDeclarations {
    $cmakeLists = Get-Content -LiteralPath (Join-Path $repoRoot "CMakeLists.txt") -Raw
    if ($cmakeLists -notmatch "project\s*\(\s*Elf3D\s+VERSION\s+$([regex]::Escape($Version))\b") {
        throw "CMakeLists.txt project version does not match $Version."
    }

    $readme = Get-Content -LiteralPath (Join-Path $repoRoot "README.md") -Raw
    if ($readme -notmatch "Current version:\s+$([regex]::Escape($Version))\.") {
        throw "README.md current version does not match $Version."
    }

    $changelog = Get-Content -LiteralPath (Join-Path $repoRoot "CHANGELOG.md") -Raw
    if ($changelog -notmatch "##\s+$([regex]::Escape($Version))\b") {
        throw "CHANGELOG.md does not contain a $Version release section."
    }

    $releaseDir = Join-Path $repoRoot "docs\releases\$Version"
    if (-not (Test-Path -LiteralPath $releaseDir -PathType Container)) {
        throw "Release directory not found: $releaseDir"
    }

    foreach ($required in @("LICENSE", "THIRD_PARTY.md", "third_party\licenses")) {
        $path = Join-Path $repoRoot $required
        if (-not (Test-Path -LiteralPath $path)) {
            throw "Required license or notice path missing: $required"
        }
    }

    Write-Host "Version declarations and license paths match $Version."
}

function Test-RemoteTagState {
    $lines = & git ls-remote --tags origin "refs/tags/$tagName" "refs/tags/$tagName^{}"
    $hasTag = [bool]$lines

    if ($TagState -eq "Absent" -and $hasTag) {
        throw "Remote tag $tagName already exists."
    }
    if ($TagState -eq "Present" -and -not $hasTag) {
        throw "Remote tag $tagName does not exist."
    }

    if ($hasTag) {
        $object = $lines | Where-Object { $_ -match "refs/tags/$([regex]::Escape($tagName))$" } | Select-Object -First 1
        $target = $lines | Where-Object { $_ -match "refs/tags/$([regex]::Escape($tagName))\^\{\}$" } | Select-Object -First 1
        if (-not $object -or -not $target) {
            throw "Remote tag $tagName is not an annotated tag with a peeled target."
        }
        Write-Host "Remote tag object: $object"
        Write-Host "Remote tag target: $target"
    } else {
        Write-Host "Remote tag $tagName is absent."
    }
}

function Invoke-CMakeMatrix {
    $cmake = Get-RequiredTool "cmake"
    $ctest = Get-RequiredTool "ctest"
    foreach ($preset in @("windows-debug", "windows-release")) {
        Invoke-External $cmake @("--fresh", "--preset", $preset)
        Invoke-External $cmake @("--build", "--preset", $preset, "--parallel")

        Write-Host "==> $ctest --preset $preset --output-on-failure"
        $output = & $ctest --preset $preset --output-on-failure 2>&1
        $exitCode = $LASTEXITCODE
        $output | ForEach-Object { Write-Host $_ }
        if ($exitCode -ne 0) {
            throw "CTest failed for preset '$preset' with exit code $exitCode."
        }
        $summary = $output | Select-String -Pattern "tests failed out of|tests passed"
        if ($summary) {
            Write-Host "CTest summary for ${preset}: $($summary[-1].Line)"
        }
    }
}

function Test-PackageOutput {
    $zipPath = Join-Path $repoRoot "out\release\elf3d-viewer-$Version-windows-x64.zip"
    $checksumsPath = Join-Path $repoRoot "out\release\SHA256SUMS.txt"
    if (-not (Test-Path -LiteralPath $zipPath -PathType Leaf)) {
        throw "Expected package not found: $zipPath"
    }
    if (-not (Test-Path -LiteralPath $checksumsPath -PathType Leaf)) {
        throw "Expected checksum file not found: $checksumsPath"
    }

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [System.IO.Compression.ZipFile]::OpenRead($zipPath)
    try {
        $entries = $zip.Entries | ForEach-Object { $_.FullName }
    } finally {
        $zip.Dispose()
    }
    $normalizedEntries = $entries | ForEach-Object { $_ -replace "\\", "/" }

    foreach ($required in @("elf3d_viewer.exe", "elf3d.dll", "LICENSE", "THIRD_PARTY.md", "README.txt")) {
        if (-not ($normalizedEntries | Where-Object { $_ -eq $required })) {
            throw "Package is missing '$required'."
        }
    }
    foreach ($requiredAsset in @(
            "assets/font/DroidSans.ttf",
            "assets/icon/open.png",
            "assets/icon/reload.png",
            "assets/icon/fit_view.png",
            "assets/icon/reset_camera.png",
            "assets/icon/select.png",
            "assets/icon/measure.png",
            "assets/icon/clipping_panel.png",
            "assets/icon/section_plane.png",
            "assets/icon/add_clipping_box.png",
            "assets/icon/clear_clipping.png",
            "assets/icon/hide_selected.png",
            "assets/icon/show_selected.png",
            "assets/icon/isolate_selected.png",
            "assets/icon/show_all.png",
            "assets/icon/reset_layout.png"
        )) {
        if (-not ($normalizedEntries | Where-Object { $_ -eq $requiredAsset })) {
            throw "Package is missing '$requiredAsset'."
        }
    }
    if (-not ($normalizedEntries | Where-Object { $_ -like "third_party_licenses/*" })) {
        throw "Package is missing third_party_licenses contents."
    }

    $hash = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
    $checksumText = Get-Content -LiteralPath $checksumsPath -Raw
    if ($checksumText -notmatch [regex]::Escape($hash)) {
        throw "SHA256SUMS.txt does not contain the package hash."
    }
    Write-Host "Package inspection passed for $zipPath."
}

function Test-BranchSynchronization {
    if (-not $RequireSynchronizedBranches) {
        return
    }

    $main = (& git rev-parse origin/main).Trim()
    $develop = (& git rev-parse origin/develop).Trim()
    Write-Host "origin/main:    $main"
    Write-Host "origin/develop: $develop"
    if ($main -ne $develop) {
        throw "origin/main and origin/develop are not synchronized."
    }
}

function Test-GitHubRelease {
    if (-not $RequireGitHubRelease) {
        return
    }

    $uri = "https://api.github.com/repos/zavelski/elf3d/releases/tags/$tagName"
    try {
        $release = Invoke-RestMethod -Uri $uri -Headers @{ "User-Agent" = "Codex" }
    } catch {
        throw "GitHub Release $tagName was not found or could not be read: $($_.Exception.Message)"
    }

    if ($release.tag_name -ne $tagName) {
        throw "GitHub Release tag mismatch: expected $tagName, got $($release.tag_name)."
    }
    if (-not $release.assets -or $release.assets.Count -eq 0) {
        throw "GitHub Release $tagName has no assets."
    }

    Write-Host "GitHub Release: $($release.html_url)"
    foreach ($asset in $release.assets) {
        Write-Host "Asset: $($asset.name) size=$($asset.size)"
    }
}

Push-Location $repoRoot
try {
    Test-ExpectedRepository
    Test-CleanWorkingTree
    Test-VersionDeclarations
    Test-RemoteTagState

    if (-not $SkipBuild) {
        Invoke-CMakeMatrix
    }

    if (-not $SkipPackage) {
        Invoke-External powershell @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", ".\scripts\package_release.ps1", "-Version", $Version)
        Test-PackageOutput
    }

    Test-BranchSynchronization
    Test-GitHubRelease
    Write-Host "Release verification completed for $Version."
} finally {
    Pop-Location
}
