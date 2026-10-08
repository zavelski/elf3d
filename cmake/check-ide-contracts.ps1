# Helpers for check-preset-contracts.ps1; these inspect generated files only.
. (Join-Path $PSScriptRoot 'solution-projects.ps1')

function Assert-GeneratedToolchain {
    param([string]$BuildDirectory)
    $legacySolutions = @(Get-ChildItem -LiteralPath $BuildDirectory -Recurse -File -Filter '*.sln')
    if ($legacySolutions.Count) { throw 'A generated build tree contains legacy solutions. Recreate the whole build directory.' }
    $solutions = @(Get-ChildItem -LiteralPath $BuildDirectory -Recurse -File -Filter '*.slnx')
    if (-not $solutions.Count) { throw 'No XML solutions were generated.' }
    foreach ($solution in $solutions) { $null = @(Read-SolutionProjects $solution.FullName) }
    $cache = Get-Content -LiteralPath (Join-Path $BuildDirectory 'CMakeCache.txt') -Raw
    if ($cache -notmatch '(?m)^CMAKE_GENERATOR_TOOLSET:INTERNAL=v145,host=x64\r?$') {
        throw 'The effective generated toolset must be v145 with x64 host tools.'
    }
    $compilerFiles = @(Get-ChildItem -Path (Join-Path $BuildDirectory 'CMakeFiles/*/CMakeCXXCompiler.cmake'))
    if ($compilerFiles.Count -ne 1) { throw 'Expected exactly one effective C++ compiler record.' }
    $compiler = Get-Content -LiteralPath $compilerFiles[0].FullName -Raw
    if ($compiler -notmatch 'set\(CMAKE_CXX_COMPILER_VERSION "([0-9.]+)"\)' -or
        [version]$Matches[1] -lt [version]'19.50') { throw 'Effective MSVC is older than 19.50.' }
    if ($cache -notmatch '(?m)^CMAKE_GENERATOR_INSTANCE:[^=]+=(.+)\r?$') { throw 'Visual Studio instance was not recorded.' }
    $msbuild = Join-Path $Matches[1].Trim() 'MSBuild/Current/Bin/amd64/MSBuild.exe'
    $version = @(& $msbuild -nologo -version) -join ''
    if ($LASTEXITCODE -ne 0 -or [version]$version -lt [version]'18.0') { throw 'Effective MSBuild is older than 18.' }
    $cppProject = Get-ChildItem -LiteralPath $BuildDirectory -Recurse -File -Filter '*.vcxproj' |
        Select-Object -First 1
    if ($null -eq $cppProject) { throw 'A generated C++ project is missing.' }
    $propertyOutput = @(& $msbuild $cppProject.FullName /nologo /p:Configuration=Debug /p:Platform=x64 `
        '-getProperty:VCTargetsPath,VCToolsInstallDir,WindowsSdkDir,WindowsTargetPlatformVersion,PlatformToolset,Platform') -join "`n"
    if ($LASTEXITCODE -ne 0) { throw 'MSBuild could not evaluate imported C++ property/target files.' }
    $properties = ($propertyOutput | ConvertFrom-Json).Properties
    foreach ($name in @('VCTargetsPath', 'VCToolsInstallDir', 'WindowsSdkDir')) {
        $path = $properties.$name
        if (-not (Test-Path -LiteralPath $path -PathType Container) -or $path -match 'Microsoft Visual Studio[/\\]+2022') {
            throw "Invalid effective $name from MSBuild: '$path'."
        }
    }
    if ($properties.PlatformToolset -ne 'v145' -or $properties.Platform -ne 'x64' -or
        -not $properties.WindowsTargetPlatformVersion) { throw 'Unexpected effective C++ toolset/platform/SDK properties.' }
    $settings = @(Get-ChildItem -LiteralPath $BuildDirectory -Recurse -File |
        Where-Object { $_.Extension -in '.vcxproj', '.props', '.targets', '.user' })
    foreach ($file in $settings) {
        $text = Get-Content -LiteralPath $file.FullName -Raw
        if ($text -match 'Microsoft Visual Studio[/\\]+2022|<PlatformToolset>v143</PlatformToolset>') {
            throw "Obsolete toolchain reference in '$($file.FullName)'."
        }
        if ($file.Extension -ne '.vcxproj') { continue }
        [xml]$project = $text
        Assert-PchContract $project $file.FullName $cache
        $toolsets = @($project.SelectNodes("//*[local-name()='PlatformToolset']"))
        if (-not $toolsets.Count -or @($toolsets | Where-Object InnerText -NE 'v145').Count) {
            throw "Project '$($file.FullName)' does not use v145 in every configuration."
        }
        foreach ($configuration in $project.SelectNodes("//*[local-name()='ProjectConfiguration']")) {
            if ($configuration.GetAttribute('Include') -notmatch '\|x64$') { throw 'A generated project targets a non-x64 platform.' }
        }
        foreach ($group in $project.SelectNodes("//*[local-name()='ItemDefinitionGroup']")) {
            $crt = $group.SelectSingleNode(".//*[local-name()='RuntimeLibrary']")
            if ($null -eq $crt) { continue } # Utility projects do not compile sources.
            $expected = if ($group.GetAttribute('Condition') -like "*'Debug|x64'*") { 'MultiThreadedDebugDLL' } else { 'MultiThreadedDLL' }
            if ($crt.InnerText -ne $expected) { throw "Wrong CRT in '$($file.FullName)'." }
        }
    }
    if (($cache + $compiler) -match 'Microsoft Visual Studio[/\\]+2022') { throw 'A compiler/cache path still selects the old IDE.' }
}

function Assert-PchContract {
    param([xml]$Project, [string]$ProjectPath, [string]$Cache)
    $name = [IO.Path]::GetFileNameWithoutExtension($ProjectPath)
    if ($name -in @('CompilerIdC', 'CompilerIdCXX')) { return }
    $enabled = $name -in @('elf3d_foundation_modules', 'elf3d_domain_modules',
        'elf3d_model_modules', 'elf3d_gltf_modules', 'elf3d_opengl_modules',
        'elf3d_interaction_modules', 'elf3d_view_modules', 'elf3d',
        'elf3d_imgui', 'elf3d_third_party_imgui', 'elf3d_app', 'elf3d_viewer')
    if ($Cache -match '(?m)^CMAKE_DISABLE_PRECOMPILE_HEADERS:[^=]+=(?:ON|TRUE|1)\r?$') { $enabled = $false }
    foreach ($configuration in @('Debug', 'Release')) {
        $groups = @($Project.SelectNodes("//*[local-name()='ItemDefinitionGroup']") |
            Where-Object { $_.GetAttribute('Condition') -like "*'$configuration|x64'*" })
        foreach ($group in $groups) {
            $compile = $group.SelectSingleNode("./*[local-name()='ClCompile']")
            if ($null -eq $compile) { continue }
            $scan = $compile.SelectSingleNode("./*[local-name()='ScanSourceForModuleDependencies']")
            if ($null -eq $scan -or $scan.InnerText -ne 'false') { throw "Module scanning is enabled: $name $configuration." }
            $pch = $compile.SelectSingleNode("./*[local-name()='PrecompiledHeader']")
            $actual = $null -ne $pch -and $pch.InnerText -eq 'Use'
            if ($actual -ne $enabled) { throw "Incorrect PCH setting: $name $configuration." }
            if ($enabled) {
                $create = @($Project.SelectNodes("//*[local-name()='ClCompile'][@Include]/*[local-name()='PrecompiledHeader']") |
                    Where-Object { $_.InnerText -eq 'Create' -and $_.GetAttribute('Condition') -like "*'$configuration|x64'*" })
                $output = $compile.SelectSingleNode("./*[local-name()='PrecompiledHeaderOutputFile']")
                if ($create.Count -ne 1 -or $null -eq $output -or $output.InnerText -notlike "*$name.dir/$configuration/*") {
                    throw "PCH is not owned by its target/configuration: $name $configuration."
                }
            }
        }
    }
}

function Assert-IdeContract {
    param([string]$BuildDirectory, [string]$RepositoryPath, [bool]$Engine)
    $solutionPath = Join-Path $BuildDirectory "Elf3D.slnx"
    Assert-GeneratedToolchain $BuildDirectory
    $projects = @(Read-SolutionProjects $solutionPath)
    $sourcePrefix = $RepositoryPath.TrimEnd('\', '/') + '\'
    $headers = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase
    )
    foreach ($entry in $projects) {
        if ($entry.Name -notmatch '^elf3d($|_(model$|.*_modules$|app$|imgui$|viewer$))') {
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
    $viewerProject = Get-SolutionStartupProject $projects
    if ($viewerProject.Name -ne 'elf3d_viewer') {
        throw "The standalone solution must select elf3d_viewer as its default startup project."
    }
    [xml]$viewer = Get-Content -LiteralPath $viewerProject.Path -Raw
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
}

function Assert-ExternalApplicationContract {
    param([string]$BuildDirectory, [string]$RepositoryPath)
    Assert-GeneratedToolchain $BuildDirectory
    $cache = Get-Content -LiteralPath (Join-Path $BuildDirectory 'CMakeCache.txt') -Raw
    foreach ($setting in @('BUILD_TESTING:BOOL=ON', 'ELF3D_BUILD_TESTING:BOOL=OFF', 'ELF3D_BUILD_VIEWER:BOOL=OFF')) {
        if ($cache -notmatch "(?m)^$([regex]::Escape($setting))\r?$") {
            throw "External application has an incorrect default: $setting."
        }
    }
    if ($cache -match 'ELF3D_ENABLE_GLTF_CORPUS_TESTS:') {
        throw 'Dependency configure loaded internal local-validation options.'
    }
    $projects = @(Read-SolutionProjects (Join-Path $BuildDirectory 'Elf3DExternalApplication.slnx'))
    $dependencyProjects = @(Read-SolutionProjects (Join-Path $BuildDirectory 'elf3d/Elf3D.slnx'))
    $clientProject = Get-SolutionStartupProject $projects
    if ($clientProject.Name -ne 'elf3d_external_application' -or
        @($dependencyProjects | Where-Object { $_.Name -match '^elf3d_.*tests?$|^elf3d_viewer$' }).Count -ne 0) {
        throw 'Elf3D changed the parent startup project or added default tests/viewer.'
    }
    [xml]$client = Get-Content -LiteralPath $clientProject.Path -Raw
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
