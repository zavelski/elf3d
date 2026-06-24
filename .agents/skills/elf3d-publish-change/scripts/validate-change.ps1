[CmdletBinding()]
param(
    [ValidateSet("Docs", "Code", "Full", "Package")]
    [string]$Mode = "Full",

    [string]$PackageVersion = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\..\..\.."))

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
    Write-Host "Repository status:"
    & git status --short --branch
    Write-Host "Ignored project outputs:"
    & git status --ignored --short
}

function Test-PowerShellSyntax {
    $errors = $null
    $files = @()
    $files += Get-ChildItem -LiteralPath (Join-Path $repoRoot "scripts") -Filter "*.ps1" -File -ErrorAction SilentlyContinue
    $agentsPath = Join-Path $repoRoot ".agents"
    if (Test-Path -LiteralPath $agentsPath) {
        $files += Get-ChildItem -LiteralPath $agentsPath -Recurse -Filter "*.ps1" -File
    }

    foreach ($file in $files | Sort-Object -Property FullName -Unique) {
        $errors = $null
        [System.Management.Automation.PSParser]::Tokenize((Get-Content -LiteralPath $file.FullName -Raw), [ref]$errors) | Out-Null
        if ($errors -and $errors.Count -gt 0) {
            $messages = $errors | ForEach-Object { "$($_.Message) at line $($_.Token.StartLine)" }
            throw "PowerShell parse failed for $($file.FullName): $($messages -join '; ')"
        }
    }

    Write-Host "PowerShell syntax check passed for $($files.Count) script(s)."
}

function Test-YamlSyntax {
    $python = Get-Command python -ErrorAction SilentlyContinue
    if (-not $python) {
        Write-Warning "python was not found; skipping YAML syntax checks."
        return
    }

    $code = @'
import pathlib
import sys
try:
    import yaml
except Exception as exc:
    print(f"PyYAML unavailable: {exc}")
    sys.exit(2)

root = pathlib.Path.cwd()
paths = list((root / ".github").glob("workflows/*.yml"))
paths += list((root / ".agents").glob("skills/*/SKILL.md")) if (root / ".agents").exists() else []
for path in paths:
    text = path.read_text(encoding="utf-8")
    if path.name == "SKILL.md":
        if not text.startswith("---"):
            raise SystemExit(f"{path}: missing front matter")
        end = text.find("\n---", 4)
        if end < 0:
            raise SystemExit(f"{path}: malformed front matter")
        text = text[4:end]
    yaml.safe_load(text)
print(f"YAML syntax check passed for {len(paths)} file(s).")
'@

    & $python.Source -c $code
    if ($LASTEXITCODE -eq 2) {
        Write-Warning "PyYAML is unavailable; skipping YAML syntax checks."
    } elseif ($LASTEXITCODE -ne 0) {
        throw "YAML syntax check failed."
    }
}

function Test-MarkdownRelativeLinks {
    $roots = @("AGENTS.md", "README.md", "CONTRIBUTING.md", "SECURITY.md", "CHANGELOG.md", "docs", ".agents")
    $files = @()
    foreach ($root in $roots) {
        $path = Join-Path $repoRoot $root
        if (Test-Path -LiteralPath $path -PathType Leaf) {
            $files += Get-Item -LiteralPath $path
        } elseif (Test-Path -LiteralPath $path -PathType Container) {
            $files += Get-ChildItem -LiteralPath $path -Recurse -Filter "*.md" -File
        }
    }

    $missing = New-Object System.Collections.Generic.List[string]
    $linkPattern = "\[[^\]]+\]\(([^)]+)\)"
    foreach ($file in $files | Sort-Object -Property FullName -Unique) {
        $content = Get-Content -LiteralPath $file.FullName -Raw
        foreach ($match in [regex]::Matches($content, $linkPattern)) {
            $target = $match.Groups[1].Value.Trim()
            if ($target.StartsWith("#") -or $target -match "^[a-zA-Z][a-zA-Z0-9+.-]*:" -or $target.StartsWith("mailto:")) {
                continue
            }
            if ($target.StartsWith("<") -and $target.EndsWith(">")) {
                $target = $target.Substring(1, $target.Length - 2)
            }
            $target = ($target -split "#", 2)[0]
            if ([string]::IsNullOrWhiteSpace($target)) {
                continue
            }
            $candidate = [System.IO.Path]::GetFullPath((Join-Path $file.DirectoryName $target))
            if (-not (Test-Path -LiteralPath $candidate)) {
                $missing.Add("$($file.FullName): missing link target '$target'")
            }
        }
    }

    if ($missing.Count -gt 0) {
        throw "Markdown link check failed:`n$($missing -join "`n")"
    }
    Write-Host "Markdown relative link check passed for $($files.Count) file(s)."
}

function Test-TextValidation {
    Invoke-External git @("diff", "--check")

    $presetsPath = Join-Path $repoRoot "CMakePresets.json"
    Get-Content -LiteralPath $presetsPath -Raw | ConvertFrom-Json | Out-Null
    Write-Host "CMakePresets.json parsed successfully."

    Test-PowerShellSyntax
    Test-YamlSyntax
    Test-MarkdownRelativeLinks
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

function Get-ProjectVersion {
    $cmakeLists = Get-Content -LiteralPath (Join-Path $repoRoot "CMakeLists.txt") -Raw
    $match = [regex]::Match($cmakeLists, "project\s*\(\s*Elf3D\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)")
    if (-not $match.Success) {
        throw "Could not determine project version from CMakeLists.txt."
    }
    return $match.Groups[1].Value
}

function Test-PackageOutput {
    param([Parameter(Mandatory = $true)][string]$Version)

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

    foreach ($required in @("elf3d_viewer.exe", "elf3d.dll", "LICENSE", "THIRD_PARTY.md", "README.txt")) {
        if (-not ($entries | Where-Object { $_ -eq $required })) {
            throw "Package is missing '$required'."
        }
    }
    if (-not ($entries | Where-Object { $_ -like "third_party_licenses/*" })) {
        throw "Package is missing third_party_licenses contents."
    }

    $hash = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
    $checksumText = Get-Content -LiteralPath $checksumsPath -Raw
    if ($checksumText -notmatch [regex]::Escape($hash)) {
        throw "SHA256SUMS.txt does not contain the package hash."
    }
    Write-Host "Package inspection passed for $zipPath."
}

Push-Location $repoRoot
try {
    Test-ExpectedRepository
    Test-TextValidation

    if ($Mode -in @("Code", "Full", "Package")) {
        Invoke-CMakeMatrix
    }

    if ($Mode -eq "Package") {
        $version = $PackageVersion
        if ([string]::IsNullOrWhiteSpace($version)) {
            $version = Get-ProjectVersion
        }
        Invoke-External powershell @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", ".\scripts\package_release.ps1", "-Version", $version)
        Test-PackageOutput -Version $version
    }

    Write-Host "Validation completed for mode '$Mode'."
} finally {
    Pop-Location
}
