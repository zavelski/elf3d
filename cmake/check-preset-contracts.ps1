[CmdletBinding()]
param(
    [string]$RepositoryRoot = "",
    [switch]$KeepBuilds
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
    $RepositoryRoot = Split-Path -Parent $PSScriptRoot
}

function Get-NamedEntry {
    param(
        [Parameter(Mandatory)]
        [object[]]$Entries,
        [Parameter(Mandatory)]
        [string]$Name,
        [Parameter(Mandatory)]
        [string]$Kind
    )

    $matches = @($Entries | Where-Object { $_.name -eq $Name })
    if ($matches.Count -ne 1) {
        throw "Expected exactly one $Kind named '$Name', found $($matches.Count)."
    }
    return $matches[0]
}

function Assert-CacheValue {
    param(
        [Parameter(Mandatory)]
        [object]$Preset,
        [Parameter(Mandatory)]
        [string]$Variable,
        [Parameter(Mandatory)]
        [string]$Expected
    )

    $property = $Preset.cacheVariables.PSObject.Properties[$Variable]
    if ($null -eq $property) {
        throw "Configure preset '$($Preset.name)' must explicitly set $Variable=$Expected."
    }
    $actual = [string]$property.Value
    if ($actual -ne $Expected) {
        throw "Configure preset '$($Preset.name)' sets $Variable=$actual; expected $Expected."
    }
}

function Read-ConfiguredTargets {
    param(
        [Parameter(Mandatory)]
        [string]$BuildDirectory
    )

    $replyDirectory = Join-Path $BuildDirectory ".cmake/api/v1/reply"
    $indexFile = Get-ChildItem -LiteralPath $replyDirectory -Filter "index-*.json" |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if ($null -eq $indexFile) {
        throw "CMake File API did not produce an index below '$replyDirectory'."
    }

    $index = Get-Content -LiteralPath $indexFile.FullName -Raw | ConvertFrom-Json
    $codemodelReply = $index.reply.PSObject.Properties["codemodel-v2"]
    if ($null -eq $codemodelReply) {
        throw "CMake File API index '$($indexFile.FullName)' has no codemodel-v2 reply."
    }

    $codemodelFile = Join-Path $replyDirectory $codemodelReply.Value.jsonFile
    $codemodel = Get-Content -LiteralPath $codemodelFile -Raw | ConvertFrom-Json
    $targets = foreach ($configuration in $codemodel.configurations) {
        foreach ($target in $configuration.targets) {
            [string]$target.name
        }
    }
    return @($targets | Sort-Object -Unique)
}

function Assert-TargetContract {
    param(
        [Parameter(Mandatory)]
        [string]$PresetName,
        [Parameter(Mandatory)]
        [string[]]$Targets,
        [Parameter(Mandatory)]
        [AllowEmptyCollection()]
        [string[]]$RequiredTargets,
        [Parameter(Mandatory)]
        [AllowEmptyCollection()]
        [string[]]$ForbiddenTargets
    )

    foreach ($required in $RequiredTargets) {
        if ($required -notin $Targets) {
            throw "Configure preset '$PresetName' did not create required target '$required'."
        }
    }
    foreach ($forbidden in $ForbiddenTargets) {
        if ($forbidden -in $Targets) {
            throw "Configure preset '$PresetName' unexpectedly created target '$forbidden'."
        }
    }
}

function Remove-ValidationTree {
    param(
        [Parameter(Mandatory)]
        [string]$Path,
        [Parameter(Mandatory)]
        [string]$AllowedPrefix,
        [Parameter(Mandatory)]
        [string]$AllowedRoot
    )

    $resolvedPath = [System.IO.Path]::GetFullPath((Resolve-Path -LiteralPath $Path).Path)
    if (-not $resolvedPath.StartsWith(
            $AllowedPrefix,
            [System.StringComparison]::OrdinalIgnoreCase
        )) {
        throw "Refusing to remove preset-contract path outside '$AllowedRoot'."
    }

    $maximumAttempts = 20
    for ($attempt = 1; $attempt -le $maximumAttempts; ++$attempt) {
        try {
            Remove-Item -LiteralPath $resolvedPath -Recurse -Force -ErrorAction Stop
            return
        }
        catch {
            if ($attempt -eq $maximumAttempts) {
                throw
            }
            Start-Sleep -Milliseconds 250
        }
    }
}

$repositoryPath = [System.IO.Path]::GetFullPath($RepositoryRoot)
$presetPath = Join-Path $repositoryPath "CMakePresets.json"
if (-not (Test-Path -LiteralPath $presetPath -PathType Leaf)) {
    throw "CMake preset file was not found at '$presetPath'."
}
if ($null -eq (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "CMake is required to validate preset contracts."
}

$contracts = @(
    [pscustomobject]@{
        Name = "windows-debug"
        Configuration = "Debug"
        Engine = "ON"
        Viewer = "ON"
        Benchmark = "ON"
        RequiredTargets = @(
            "elf3d_model",
            "elf3d",
            "elf3d_imgui",
            "elf3d_viewer",
            "elf3d_render_benchmark"
        )
        ForbiddenTargets = @()
    },
    [pscustomobject]@{
        Name = "windows-release"
        Configuration = "Release"
        Engine = "ON"
        Viewer = "ON"
        Benchmark = "ON"
        RequiredTargets = @(
            "elf3d_model",
            "elf3d",
            "elf3d_imgui",
            "elf3d_viewer",
            "elf3d_render_benchmark"
        )
        ForbiddenTargets = @()
    },
    [pscustomobject]@{
        Name = "windows-model-debug"
        Configuration = "Debug"
        Engine = "OFF"
        Viewer = "OFF"
        Benchmark = "OFF"
        RequiredTargets = @(
            "elf3d_model",
            "elf3d_foundation_modules",
            "elf3d_image_modules",
            "elf3d_model_modules",
            "elf3d_gltf_modules"
        )
        ForbiddenTargets = @(
            "elf3d",
            "elf3d_imgui",
            "elf3d_viewer",
            "elf3d_render_benchmark",
            "elf3d_domain_modules",
            "elf3d_graphics_modules",
            "elf3d_opengl_modules",
            "elf3d_interaction_modules",
            "elf3d_view_modules"
        )
    },
    [pscustomobject]@{
        Name = "windows-model-release"
        Configuration = "Release"
        Engine = "OFF"
        Viewer = "OFF"
        Benchmark = "OFF"
        RequiredTargets = @(
            "elf3d_model",
            "elf3d_foundation_modules",
            "elf3d_image_modules",
            "elf3d_model_modules",
            "elf3d_gltf_modules"
        )
        ForbiddenTargets = @(
            "elf3d",
            "elf3d_imgui",
            "elf3d_viewer",
            "elf3d_render_benchmark",
            "elf3d_domain_modules",
            "elf3d_graphics_modules",
            "elf3d_opengl_modules",
            "elf3d_interaction_modules",
            "elf3d_view_modules"
        )
    }
)

$presets = Get-Content -LiteralPath $presetPath -Raw | ConvertFrom-Json
$minimum = $presets.cmakeMinimumRequired
if ($minimum.major -ne 4 -or $minimum.minor -ne 3 -or $minimum.patch -ne 4) {
    throw "CMakePresets.json must declare the supported CMake baseline 4.3.4."
}
foreach ($contract in $contracts) {
    $configurePreset = Get-NamedEntry -Entries @($presets.configurePresets) `
        -Name $contract.Name -Kind "configure preset"
    if ($configurePreset.generator -ne "Visual Studio 17 2022" -or
        $configurePreset.architecture -ne "x64") {
        throw "Configure preset '$($contract.Name)' must use Visual Studio 2022 x64."
    }
    Assert-CacheValue -Preset $configurePreset -Variable "BUILD_TESTING" -Expected "ON"
    Assert-CacheValue -Preset $configurePreset -Variable "CMAKE_CONFIGURATION_TYPES" `
        -Expected "Debug;Release"
    Assert-CacheValue -Preset $configurePreset -Variable "ELF3D_BUILD_ENGINE" `
        -Expected $contract.Engine
    Assert-CacheValue -Preset $configurePreset -Variable "ELF3D_BUILD_VIEWER" `
        -Expected $contract.Viewer
    Assert-CacheValue -Preset $configurePreset -Variable "ELF3D_BUILD_PERFORMANCE_BENCHMARK" `
        -Expected $contract.Benchmark

    $buildPreset = Get-NamedEntry -Entries @($presets.buildPresets) `
        -Name $contract.Name -Kind "build preset"
    if ($buildPreset.configurePreset -ne $contract.Name -or
        $buildPreset.configuration -ne $contract.Configuration) {
        throw "Build preset '$($contract.Name)' does not match its configure/configuration contract."
    }

    $testPreset = Get-NamedEntry -Entries @($presets.testPresets) `
        -Name $contract.Name -Kind "test preset"
    if ($testPreset.configurePreset -ne $contract.Name -or
        $testPreset.configuration -ne $contract.Configuration) {
        throw "Test preset '$($contract.Name)' does not match its configure/configuration contract."
    }
}

$outRoot = [System.IO.Path]::GetFullPath((Join-Path $repositoryPath "out"))
$runRoot = [System.IO.Path]::GetFullPath(
    (Join-Path $outRoot ("preset-contract-" + [System.Guid]::NewGuid().ToString("N")))
)
$outPrefix = $outRoot.TrimEnd(
    [System.IO.Path]::DirectorySeparatorChar,
    [System.IO.Path]::AltDirectorySeparatorChar
) + [System.IO.Path]::DirectorySeparatorChar
if (-not $runRoot.StartsWith($outPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Preset-contract build root '$runRoot' is outside the repository output directory."
}

New-Item -ItemType Directory -Path $runRoot -Force | Out-Null
try {
    foreach ($contract in $contracts) {
        $buildDirectory = Join-Path $runRoot $contract.Name
        $queryDirectory = Join-Path $buildDirectory ".cmake/api/v1/query"
        New-Item -ItemType Directory -Path $queryDirectory -Force | Out-Null
        New-Item -ItemType File -Path (Join-Path $queryDirectory "codemodel-v2") -Force |
            Out-Null

        Write-Host "Configuring preset contract '$($contract.Name)'..."
        & cmake --preset $contract.Name -S $repositoryPath -B $buildDirectory --fresh
        if ($LASTEXITCODE -ne 0) {
            throw "CMake configure failed for preset '$($contract.Name)' with exit code $LASTEXITCODE."
        }

        $targets = Read-ConfiguredTargets -BuildDirectory $buildDirectory
        Assert-TargetContract -PresetName $contract.Name -Targets $targets `
            -RequiredTargets $contract.RequiredTargets -ForbiddenTargets $contract.ForbiddenTargets
    }
    Write-Host "All Elf3D preset contracts passed."
}
finally {
    if ($KeepBuilds) {
        Write-Host "Preset-contract build trees retained at '$runRoot'."
    }
    elseif (Test-Path -LiteralPath $runRoot -PathType Container) {
        Remove-ValidationTree -Path $runRoot -AllowedPrefix $outPrefix -AllowedRoot $outRoot
    }
}
