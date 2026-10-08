[CmdletBinding()]
param(
    [string]$RepositoryRoot = "",
    [switch]$KeepBuilds
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot 'check-ide-contracts.ps1')
& (Join-Path $PSScriptRoot 'check-solution-contracts.ps1')

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

function Resolve-ConfigurePreset {
    param([object]$Preset, [object[]]$Entries)
    $result = [ordered]@{}
    $cache = @{}
    if ($Preset.PSObject.Properties['inherits']) {
        foreach ($parentName in @($Preset.inherits)) {
            $parent = Resolve-ConfigurePreset (Get-NamedEntry $Entries $parentName 'configure preset') $Entries
            foreach ($property in $parent.PSObject.Properties) {
                if (-not $result.Contains($property.Name)) { $result[$property.Name] = $property.Value }
            }
            foreach ($property in $parent.cacheVariables.PSObject.Properties) {
                if (-not $cache.ContainsKey($property.Name)) { $cache[$property.Name] = $property.Value }
            }
        }
    }
    foreach ($property in $Preset.PSObject.Properties) { $result[$property.Name] = $property.Value }
    if ($Preset.PSObject.Properties['cacheVariables']) {
        foreach ($property in $Preset.cacheVariables.PSObject.Properties) { $cache[$property.Name] = $property.Value }
    }
    $result['cacheVariables'] = [pscustomobject]$cache
    return [pscustomobject]$result
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

$legacyFiles = if (Test-Path -LiteralPath (Join-Path $repositoryPath '.git')) {
    $files = @(& git -C $repositoryPath ls-files --cached --others --exclude-standard -- '*.sln')
    if ($LASTEXITCODE -ne 0) { throw 'Could not inspect source solution files.' }
    $files
} else {
    # Source archives have no Git metadata; do not inspect a parent repository.
    Get-ChildItem -LiteralPath $repositoryPath -Force |
        Where-Object { $_.Name -notin 'out', 'win', '.local' } |
        ForEach-Object {
            if ($_.PSIsContainer) {
                Get-ChildItem -LiteralPath $_.FullName -Recurse -File -Filter '*.sln'
            } elseif ($_.Extension -eq '.sln') { $_ }
        } | Select-Object -ExpandProperty FullName
}
$legacyFiles = @($legacyFiles)
if ($legacyFiles.Count) { throw "Legacy source solutions are unsupported: $($legacyFiles -join ', ')." }
foreach ($relative in @('CMakePresets.json', 'CMakeLists.txt', '.github/workflows/ci.yml',
    'examples/external_application/CMakeLists.txt', 'cmake/configure-win.ps1',
    '.agents/skills/elf3d-corpus/scripts/run-corpus.ps1')) {
    $path = Join-Path $repositoryPath $relative
    if (-not (Test-Path -LiteralPath $path)) { continue } # Internal scripts are absent in public exports.
    if ((Get-Content -LiteralPath $path -Raw) -match 'Visual Studio 17 2022|Microsoft Visual Studio[/\\]+2022|windows-2022|\bv143\b|\.sln["'']') {
        throw "Active source configuration '$relative' still selects an unsupported toolchain/solution."
    }
}

$contracts = @(
    [pscustomobject]@{
        Name = "windows-debug"
        ConfigureName = "windows-full"
        Configuration = "Debug"
        Engine = "ON"
        Viewer = "ON"
        Benchmark = "ON"
        RequiredTargets = @(
            "elf3d_model",
            "elf3d",
            "elf3d_imgui",
            "elf3d_app",
            "elf3d_viewer",
            "elf3d_render_benchmark"
        )
        ForbiddenTargets = @()
    },
    [pscustomobject]@{
        Name = "windows-release"
        ConfigureName = "windows-full"
        Configuration = "Release"
        Engine = "ON"
        Viewer = "ON"
        Benchmark = "ON"
        RequiredTargets = @(
            "elf3d_model",
            "elf3d",
            "elf3d_imgui",
            "elf3d_app",
            "elf3d_viewer",
            "elf3d_render_benchmark"
        )
        ForbiddenTargets = @()
    },
    [pscustomobject]@{
        Name = "windows-model-debug"
        ConfigureName = "windows-model"
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
            "elf3d_app",
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
        ConfigureName = "windows-model"
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
            "elf3d_app",
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
if ($minimum.major -ne 4 -or $minimum.minor -ne 4 -or $minimum.patch -ne 3) {
    throw "CMakePresets.json must declare the supported CMake baseline 4.4.3."
}
foreach ($contract in $contracts) {
    $configurePreset = Get-NamedEntry -Entries @($presets.configurePresets) `
        -Name $contract.ConfigureName -Kind "configure preset"
    $configurePreset = Resolve-ConfigurePreset $configurePreset $presets.configurePresets
    if ($configurePreset.generator -ne "Visual Studio 18 2026" -or
        $configurePreset.architecture -ne "x64" -or $configurePreset.toolset -ne "v145,host=x64") {
        throw "Configure preset '$($contract.Name)' must use Visual Studio 2026 x64."
    }
    Assert-CacheValue -Preset $configurePreset -Variable "BUILD_TESTING" -Expected "ON"
    Assert-CacheValue -Preset $configurePreset -Variable "ELF3D_BUILD_TESTING" -Expected "ON"
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
    if ($buildPreset.configurePreset -ne $contract.ConfigureName -or
        $buildPreset.configuration -ne $contract.Configuration) {
        throw "Build preset '$($contract.Name)' does not match its configure/configuration contract."
    }

    $testPreset = Get-NamedEntry -Entries @($presets.testPresets) `
        -Name $contract.Name -Kind "test preset"
    if ($testPreset.configurePreset -ne $contract.ConfigureName -or
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
    foreach ($contract in @($contracts | Group-Object ConfigureName | ForEach-Object { $_.Group[0] })) {
        $buildDirectory = Join-Path $runRoot $contract.ConfigureName
        $queryDirectory = Join-Path $buildDirectory ".cmake/api/v1/query"
        New-Item -ItemType Directory -Path $queryDirectory -Force | Out-Null
        New-Item -ItemType File -Path (Join-Path $queryDirectory "codemodel-v2") -Force |
            Out-Null

        Write-Host "Configuring preset contract '$($contract.Name)'..."
        & cmake --preset $contract.ConfigureName -S $repositoryPath -B $buildDirectory --fresh
        if ($LASTEXITCODE -ne 0) {
            throw "CMake configure failed for preset '$($contract.Name)' with exit code $LASTEXITCODE."
        }

        # Regeneration must not accumulate duplicate projects or legacy solutions.
        & cmake --preset $contract.ConfigureName -S $repositoryPath -B $buildDirectory
        if ($LASTEXITCODE -ne 0) { throw "Repeated configure failed for '$($contract.Name)'." }

        $targets = Read-ConfiguredTargets -BuildDirectory $buildDirectory
        Assert-TargetContract -PresetName $contract.Name -Targets $targets `
            -RequiredTargets $contract.RequiredTargets -ForbiddenTargets $contract.ForbiddenTargets
        Assert-TargetContract -PresetName $contract.Name -Targets $targets `
            -RequiredTargets @('elf3d_foundation_tests', 'elf3d_model_tests') -ForbiddenTargets @()
        Assert-IdeContract -BuildDirectory $buildDirectory -RepositoryPath $repositoryPath `
            -Engine ($contract.Engine -eq 'ON')
    }
    $externalBuild = Join-Path $runRoot 'external-application'
    & cmake -S (Join-Path $repositoryPath 'examples/external_application') -B $externalBuild `
        -G 'Visual Studio 18 2026' -A x64 -T v145,host=x64 "-DELF3D_SOURCE_DIR=$repositoryPath"
    if ($LASTEXITCODE -ne 0) { throw 'External application configure failed.' }
    Assert-ExternalApplicationContract -BuildDirectory $externalBuild -RepositoryPath $repositoryPath

    # A dependency must also honor explicit opt-ins without taking over the parent.
    & cmake -S (Join-Path $repositoryPath 'examples/external_application') -B $externalBuild `
        -DELF3D_BUILD_TESTING=ON -DELF3D_BUILD_VIEWER=ON
    if ($LASTEXITCODE -ne 0) { throw 'External application explicit opt-in configure failed.' }
    $externalProjects = @(Read-SolutionProjects (Join-Path $externalBuild 'Elf3DExternalApplication.slnx'))
    $dependencyProjects = @(Read-SolutionProjects (Join-Path $externalBuild 'elf3d/Elf3D.slnx'))
    if ((Get-SolutionStartupProject $externalProjects).Name -ne 'elf3d_external_application' -or
        'elf3d_viewer' -notin $dependencyProjects.Name -or 'elf3d_app_smoke_test' -notin $dependencyProjects.Name) {
        throw 'Explicit Elf3D dependency options or parent startup project were not preserved.'
    }

    # On a fresh standalone configure the Elf3D default follows BUILD_TESTING.
    $noTestsBuild = Join-Path $runRoot 'standalone-no-tests'
    & cmake --preset windows-model -S $repositoryPath -B $noTestsBuild `
        -DBUILD_TESTING=OFF -U ELF3D_BUILD_TESTING
    if ($LASTEXITCODE -ne 0) { throw 'Standalone testing-default configure failed.' }
    $cache = Get-Content -LiteralPath (Join-Path $noTestsBuild 'CMakeCache.txt') -Raw
    $noTestsProjects = @(Read-SolutionProjects (Join-Path $noTestsBuild 'Elf3D.slnx'))
    if ($cache -notmatch '(?m)^ELF3D_BUILD_TESTING:BOOL=OFF\r?$' -or
        @($noTestsProjects | Where-Object { $_.Name -match '^elf3d_.*tests$' }).Count -ne 0) {
        throw 'Standalone ELF3D_BUILD_TESTING did not default to BUILD_TESTING=OFF.'
    }
    & cmake -S $repositoryPath -B $noTestsBuild -DELF3D_BUILD_TESTING=ON
    if ($LASTEXITCODE -ne 0) { throw 'Standalone explicit testing configure failed.' }
    $enabledProjects = @(Read-SolutionProjects (Join-Path $noTestsBuild 'Elf3D.slnx'))
    if ('elf3d_model_tests' -notin $enabledProjects.Name) {
        throw 'Explicit ELF3D_BUILD_TESTING=ON must take precedence over BUILD_TESTING=OFF.'
    }
    & (Join-Path $PSScriptRoot "check-win-components.ps1") -BuildRoot (Join-Path $runRoot "components")
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
