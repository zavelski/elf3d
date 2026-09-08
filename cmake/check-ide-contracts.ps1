# Helpers for check-preset-contracts.ps1; these inspect generated files only.
function Read-SolutionProjects {
    param([string]$SolutionPath)
    $directory = Split-Path -Parent $SolutionPath
    foreach ($line in Get-Content -LiteralPath $SolutionPath) {
        if ($line -match '^Project\("[^"]+"\) = "([^"]+)", "([^"]+\.vcxproj)",') {
            [pscustomobject]@{
                Name = $Matches[1]
                RelativePath = $Matches[2]
                Path = [System.IO.Path]::GetFullPath(
                    [System.IO.Path]::Combine($directory, $Matches[2])
                )
            }
        }
    }
}

function Assert-IdeContract {
    param([string]$BuildDirectory, [string]$RepositoryPath, [bool]$Engine)
    $solutionPath = Join-Path $BuildDirectory "Elf3D.sln"
    $projects = @(Read-SolutionProjects $solutionPath)
    $sourcePrefix = $RepositoryPath.TrimEnd('\', '/') + '\'
    $headers = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase
    )
    foreach ($entry in $projects) {
        if ($entry.Name -notmatch '^elf3d($|_(model$|.*_modules$|app$|embed$|imgui$|viewer$))') {
            continue
        }
        [xml]$filters = Get-Content -LiteralPath ($entry.Path + ".filters") -Raw
        foreach ($source in $filters.SelectNodes("//*[@Include][*[local-name()='Filter']]")) {
            $path = [System.IO.Path]::GetFullPath(
                [System.IO.Path]::Combine((Split-Path -Parent $entry.Path), $source.GetAttribute("Include"))
            )
            if (-not $path.StartsWith($sourcePrefix, [System.StringComparison]::OrdinalIgnoreCase) -or
                $path.StartsWith($BuildDirectory, [System.StringComparison]::OrdinalIgnoreCase)) {
                continue
            }
            $expected = Split-Path -Parent $path.Substring($sourcePrefix.Length)
            if ($source.Filter -ne $expected) {
                throw "Wrong source group for '$path' in '$($entry.Name)': expected '$expected'."
            }
            if ($path -match '\.(h|hpp)$') {
                [void]$headers.Add($path)
                if ($Engine -and $path.StartsWith(($sourcePrefix + 'include\')) -and $entry.Name -ne 'elf3d') {
                    throw "Public header '$path' must be displayed in the facade project."
                }
            }
        }
    }

    # Discovery here checks IDE coverage only; CMake continues to use explicit source lists.
    $roots = if ($Engine) {
        @('include', 'facade/elf3d', 'apps/viewer', 'modules', 'framework/app', 'integrations')
    } else {
        @('modules/core', 'modules/math', 'modules/model', 'modules/image', 'modules/gltf', 'include/elf3d/core', 'include/elf3d/math')
    }
    $expectedHeaders = @(
        foreach ($root in $roots) {
            Get-ChildItem -LiteralPath (Join-Path $RepositoryPath $root) -Recurse -File |
                Where-Object { $_.Extension -in '.h', '.hpp' -and $_.FullName -notmatch '\\tests\\' }
        }
        if (-not $Engine) {
            foreach ($name in @('model.h', 'model_ids.h', 'model_types.h')) {
                Get-Item -LiteralPath (Join-Path $RepositoryPath "include/elf3d/$name")
            }
        }
    )
    foreach ($header in $expectedHeaders) {
        if (-not $headers.Contains($header.FullName)) {
            throw "Header '$($header.FullName)' is missing from the product IDE tree."
        }
    }
    if (-not $Engine) {
        return
    }
    if ($projects[0].Name -ne 'elf3d_viewer') {
        throw "The standalone solution must start with elf3d_viewer."
    }
    [xml]$viewer = Get-Content -LiteralPath $projects[0].Path -Raw
    foreach ($configuration in @('Debug', 'Release')) {
        $workingDirectories = @($viewer.SelectNodes("//*[local-name()='LocalDebuggerWorkingDirectory']") |
            Where-Object { $_.GetAttribute('Condition') -like "*'$configuration|x64'*" })
        $expected = (Join-Path $BuildDirectory "bin/$configuration").Replace('\', '/')
        if ($workingDirectories.Count -ne 1 -or $workingDirectories[0].InnerText.Replace('\', '/') -ne $expected) {
            throw "Viewer debugger working directory is incorrect for $configuration."
        }
    }
    if ($viewer.SelectNodes("//*[local-name()='LocalDebuggerCommandArguments']").Count -ne 0) {
        throw "Viewer must launch without a default model argument."
    }

    $solutionHash = (Get-FileHash -LiteralPath $solutionPath).Hash
    & (Join-Path $PSScriptRoot 'create-study-solution.ps1') -BuildDirectory $BuildDirectory
    if ((Get-FileHash -LiteralPath $solutionPath).Hash -ne $solutionHash) {
        throw "Creating the study filter modified the full solution."
    }
    $filter = Get-Content -LiteralPath (Join-Path $BuildDirectory 'Elf3D-Study.slnf') -Raw | ConvertFrom-Json
    if ($filter.solution.path -ne 'Elf3D.sln') {
        throw "Study filter refers to the wrong solution."
    }
    $remaining = @($filter.solution.projects)
    $expectedPaths = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    [void]$expectedPaths.Add($projects[0].Path)
    # Repeated expansion independently checks the exact closure, including utility references.
    do {
        $added = $false
        foreach ($entry in $projects) {
            if (-not $expectedPaths.Contains($entry.Path)) { continue }
            [xml]$project = Get-Content -LiteralPath $entry.Path -Raw
            foreach ($reference in $project.SelectNodes("//*[local-name()='ProjectReference'][@Include]")) {
                $path = [System.IO.Path]::GetFullPath([System.IO.Path]::Combine(
                    (Split-Path -Parent $entry.Path), $reference.GetAttribute('Include')))
                if ($expectedPaths.Add($path)) { $added = $true }
            }
        }
    } while ($added)
    foreach ($relative in $remaining) {
        $matching = @($projects | Where-Object { $_.RelativePath -eq $relative })
        if ($matching.Count -ne 1 -or -not (Test-Path -LiteralPath $matching[0].Path -PathType Leaf) -or
            -not $expectedPaths.Remove($matching[0].Path)) {
            throw "Study filter contains an invalid, duplicate, or unrelated project '$relative'."
        }
    }
    if ($expectedPaths.Count -ne 0) {
        throw "Study filter omits viewer dependencies: $($expectedPaths -join ', ')."
    }
}

function Assert-ExternalApplicationContract {
    param([string]$BuildDirectory, [string]$RepositoryPath)
    $cache = Get-Content -LiteralPath (Join-Path $BuildDirectory 'CMakeCache.txt') -Raw
    foreach ($setting in @('BUILD_TESTING:BOOL=ON', 'ELF3D_BUILD_TESTING:BOOL=OFF', 'ELF3D_BUILD_VIEWER:BOOL=OFF')) {
        if ($cache -notmatch "(?m)^$([regex]::Escape($setting))\r?$") {
            throw "External application has an incorrect default: $setting."
        }
    }
    if ($cache -match 'ELF3D_ENABLE_GLTF_CORPUS_TESTS:') {
        throw 'Dependency configure loaded internal local-validation options.'
    }
    $projects = @(Read-SolutionProjects (Join-Path $BuildDirectory 'Elf3DExternalApplication.sln'))
    $dependencyProjects = @(Read-SolutionProjects (Join-Path $BuildDirectory 'elf3d/Elf3D.sln'))
    if ($projects[0].Name -ne 'elf3d_external_application' -or
        @($dependencyProjects | Where-Object { $_.Name -match '^elf3d_.*tests?$|^elf3d_viewer$' }).Count -ne 0) {
        throw 'Elf3D changed the parent startup project or added default tests/viewer.'
    }
    [xml]$client = Get-Content -LiteralPath $projects[0].Path -Raw
    $publicIncludes = @('include', 'framework/app/include') | ForEach-Object {
        (Join-Path $RepositoryPath $_).Replace('\', '/')
    }
    foreach ($node in $client.SelectNodes("//*[local-name()='AdditionalIncludeDirectories']")) {
        foreach ($include in $node.InnerText.Split(';')) {
            if ($include -eq '%(AdditionalIncludeDirectories)') { continue }
            if ($include.Replace('\', '/') -notin $publicIncludes) {
                throw "External client received a non-public include directory: '$include'."
            }
        }
    }
    foreach ($entry in @($projects | Where-Object { $_.Name -in 'elf3d_external_application', 'elf3d', 'elf3d_app', 'glfw' })) {
        [xml]$project = Get-Content -LiteralPath $entry.Path -Raw
        foreach ($configuration in @('Debug', 'Release')) {
            $group = @($project.SelectNodes("//*[local-name()='ItemDefinitionGroup']") |
                Where-Object { $_.GetAttribute('Condition') -like "*'$configuration|x64'*" })[0]
            $crt = $group.SelectSingleNode(".//*[local-name()='RuntimeLibrary']").InnerText
            $expectedCrt = if ($configuration -eq 'Debug') { 'MultiThreadedDebugDLL' } else { 'MultiThreadedDLL' }
            if ($crt -ne $expectedCrt) { throw "Incorrect CRT on $($entry.Name): $crt." }
            $outDir = @($project.SelectNodes("//*[local-name()='OutDir']") |
                Where-Object { $_.GetAttribute('Condition') -like "*'$configuration|x64'*" })[0].InnerText
            $kind = if ($entry.Name -in 'elf3d_app', 'glfw') { 'lib' } else { 'bin' }
            $expected = (Join-Path $BuildDirectory "$kind/$configuration").Replace('\', '/').TrimEnd('/')
            if ($outDir.Replace('\', '/').TrimEnd('/') -ne $expected) {
                throw "Elf3D did not preserve the parent's output directory for $($entry.Name)."
            }
        }
    }
    if (-not (Test-Path -LiteralPath (Join-Path $BuildDirectory 'elf3d/third_party/glfw/src/glfw.vcxproj'))) {
        throw 'GLFW binary directory escaped the Elf3D binary directory.'
    }
    $listing = @(& ctest --test-dir $BuildDirectory -C Debug --show-only=json-v1) -join "`n"
    if ($LASTEXITCODE -ne 0) { throw 'Could not inspect external CTest registration.' }
    $tests = ($listing | ConvertFrom-Json).tests
    if ($tests.Count -ne 1 -or $tests[0].name -ne 'external_application_smoke') {
        throw 'External application must retain its own test without registering Elf3D tests.'
    }
}
