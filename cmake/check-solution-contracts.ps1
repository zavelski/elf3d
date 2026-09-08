#Requires -Version 7.6.5
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'solution-projects.ps1')
. (Join-Path $PSScriptRoot 'windows-tools.ps1')
$repository = Split-Path -Parent $PSScriptRoot
$outputRoot = [IO.Path]::GetFullPath((Join-Path $repository 'out'))
$root = Join-Path $outputRoot ('solution-contract-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $root 'nested') -Force | Out-Null
function Assert-Rejected {
    param([scriptblock]$Action, [string]$Diagnostic)
    $failure = $null
    try { $null = & $Action } catch { $failure = $_.Exception.Message }
    if (-not $failure -or $failure -notmatch $Diagnostic) {
        throw "Expected rejection matching '$Diagnostic'; received '$failure'."
    }
}
try {
    $solution = Join-Path $root 'Elf3D.slnx'
    $viewer = Join-Path $root 'nested/elf3d_viewer.vcxproj'
    '<Project />' | Set-Content (Join-Path $root 'utility.vcxproj')
    '<Project><ItemDefinitionGroup><ProjectReference><LinkLibraryDependencies>false</LinkLibraryDependencies></ProjectReference></ItemDefinitionGroup><ItemGroup><ProjectReference Include="../UTILITY.vcxproj" /></ItemGroup></Project>' | Set-Content $viewer
    $validXml = '<Solution><Project Path="utility.vcxproj"/><Folder Name="/App/"><Folder Name="/App/Nested/"><Project Path="nested\..\nested/elf3d_viewer.vcxproj" DefaultStartup="true"/></Folder></Folder></Solution>'
    $validXml | Set-Content $solution
    $projects = @(Read-SolutionProjects $solution)
    if ($projects.Count -ne 2 -or (Get-SolutionStartupProject $projects).Name -ne 'elf3d_viewer') {
        throw 'Root/nested project paths or explicit startup metadata were not read.'
    }
    & (Join-Path $PSScriptRoot 'create-study-solution.ps1') -BuildDirectory $root
    $filterPath = Join-Path $root 'Elf3D-Study.slnf'
    $hash = (Get-FileHash $filterPath).Hash
    & (Join-Path $PSScriptRoot 'create-study-solution.ps1') -BuildDirectory $root
    $filter = Get-Content $filterPath -Raw | ConvertFrom-Json
    if ($hash -ne (Get-FileHash $filterPath).Hash -or $filter.solution.projects.Count -ne 2 -or
        $filter.solution.path -ne 'Elf3D.slnx') { throw 'Study-filter closure or determinism failed.' }
    Assert-Rejected { Read-SolutionProjects (Join-Path $root 'absent.slnx') } 'does not exist|Cannot find'
    Assert-Rejected { Read-SolutionProjects (Join-Path $root 'legacy.sln') } 'only .slnx'
    '<Solution><Project/></Solution>' | Set-Content $solution
    Assert-Rejected { Read-SolutionProjects $solution } 'missing its Path'
    '<Solution><Project Path="absent.vcxproj"/></Solution>' | Set-Content $solution
    Assert-Rejected { Read-SolutionProjects $solution } 'does not exist'
    '<Solution><Project Path="utility.vcxproj"/><Project Path="./UTILITY.vcxproj"/></Solution>' | Set-Content $solution
    Assert-Rejected { Read-SolutionProjects $solution } 'Duplicate'
    '<Solution><Project Path="utility.vcxproj"/></Solution>' | Set-Content $solution
    Assert-Rejected { & (Join-Path $PSScriptRoot 'create-study-solution.ps1') -BuildDirectory $root } 'no elf3d_viewer'
    Assert-Rejected { Get-SolutionStartupProject @(Read-SolutionProjects $solution) } 'exactly one'
    '<Solution><Project Path="utility.vcxproj" DefaultStartup="true"/><Project Path="nested/elf3d_viewer.vcxproj" DefaultStartup="true"/></Solution>' | Set-Content $solution
    Assert-Rejected { Get-SolutionStartupProject @(Read-SolutionProjects $solution) } 'exactly one'
    $validXml | Set-Content $solution
    '<Project><ItemGroup><ProjectReference Include="outside.vcxproj"/></ItemGroup></Project>' | Set-Content $viewer
    Assert-Rejected { & (Join-Path $PSScriptRoot 'create-study-solution.ps1') -BuildDirectory $root } 'does not belong'
    foreach ($invalidReference in @('<ProjectReference/>', '<ProjectReference Include=" "/>')) {
        "<Project><ItemGroup>$invalidReference</ItemGroup></Project>" | Set-Content $viewer
        Assert-Rejected { & (Join-Path $PSScriptRoot 'create-study-solution.ps1') -BuildDirectory $root } 'missing a valid Include'
    }

    # Exercise compiler policy without depending on old IDE/toolset installation.
    $toolchain = (Join-Path $PSScriptRoot 'windows-toolchain.cmake').Replace('\', '/')
    $probe = Join-Path $root 'toolchain.cmake'
    @"
include("$toolchain")
elf3d_check_windows_compiler()
"@ | Set-Content $probe
    foreach ($case in @(
        @{Name='old-generator'; Args=@('-DCMAKE_GENERATOR=Visual Studio 17 2022'); Pass=$false},
        @{Name='old-toolset'; Args=@('-DCMAKE_GENERATOR=Visual Studio 18 2026','-DCMAKE_GENERATOR_TOOLSET=v143,host=x64'); Pass=$false},
        @{Name='old-compiler'; Args=@('-DWIN32=ON','-DMSVC=ON','-DMSVC_VERSION=1944','-DCMAKE_CXX_COMPILER_ID=MSVC'); Pass=$false},
        @{Name='native-compiler'; Args=@('-DWIN32=ON','-DMSVC=ON','-DMSVC_VERSION=1950','-DCMAKE_CXX_COMPILER_ID=Clang'); Pass=$false},
        @{Name='current'; Args=@('-DWIN32=ON','-DMSVC=ON','-DMSVC_VERSION=1950','-DCMAKE_CXX_COMPILER_ID=MSVC'); Pass=$true},
        @{Name='future'; Args=@('-DWIN32=ON','-DMSVC=ON','-DMSVC_VERSION=2000','-DCMAKE_CXX_COMPILER_ID=MSVC','-DCMAKE_GENERATOR=Visual Studio 19 2028','-DCMAKE_VS_PLATFORM_TOOLSET=v146'); Pass=$true}
    )) {
        $arguments = $case.Args
        $message = @(& cmake @arguments -P $probe 2>&1) -join "`n"
        if (($LASTEXITCODE -eq 0) -ne $case.Pass -or (-not $case.Pass -and $message -notmatch 'Elf3D.*requires|Elf3D Windows builds require')) {
            throw "Toolchain case '$($case.Name)' failed: $message"
        }
    }
    $cmakeCommand = Get-Command cmake -CommandType Application | Select-Object -First 1
    $tools = Resolve-Elf3DCMakeTools -CMakePath $cmakeCommand.Source
    Assert-Rejected { Resolve-Elf3DCMakeTools -CMakePath $tools.CMake -CTestPath $tools.CMake } 'stable ctest version'
    Write-Host 'Solution XML, study closure, discovery, and toolchain failure contracts passed.'
} finally {
    $resolved = (Resolve-Path -LiteralPath $root).Path
    if (-not $resolved.StartsWith($outputRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Refusing cleanup outside the repository output directory.'
    }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
