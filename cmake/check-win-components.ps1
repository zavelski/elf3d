#Requires -Version 7.6.5
[CmdletBinding()]
param([string]$BuildRoot = '', [switch]$Build)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'check-ide-contracts.ps1')
$repository = Split-Path -Parent $PSScriptRoot
$out = [IO.Path]::GetFullPath((Join-Path $repository 'out'))
if (-not $BuildRoot) { $BuildRoot = Join-Path $out ('component-contract-' + [guid]::NewGuid().ToString('N')) }
$root = [IO.Path]::GetFullPath($BuildRoot)
if (-not $root.StartsWith($out + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Component validation must use an isolated root below out; the manual win tree is reserved for the user.'
}
New-Item -ItemType Directory -Path $root -Force | Out-Null
$contracts = [ordered]@{
    Dependencies = @('elf3d_third_party_zlib', 'elf3d_third_party_png', 'elf3d_third_party_jpeg',
        'elf3d_third_party_cgltf', 'elf3d_third_party_mikktspace', 'elf3d_third_party_glad', 'glfw')
    Engine = @('elf3d', 'elf3d_model', 'elf3d_foundation_modules', 'elf3d_domain_modules',
        'elf3d_image_modules', 'elf3d_model_modules', 'elf3d_gltf_modules', 'elf3d_graphics_modules',
        'elf3d_opengl_modules', 'elf3d_interaction_modules', 'elf3d_view_modules')
    ImGui = @('elf3d_third_party_imgui', 'elf3d_imgui')
    Viewer = @('elf3d_app', 'elf3d_viewer')
}
& (Join-Path $PSScriptRoot 'configure-win.ps1') -BuildRoot $root
foreach ($name in $contracts.Keys) {
    $directory = Join-Path $root $name.ToLowerInvariant()
    $solution = Join-Path $directory "Elf3D-$name.slnx"
    Assert-GeneratedToolchain $directory
    $projects = @(Read-SolutionProjects $solution)
    $actual = @($projects.Name | Where-Object { $_ -notin 'ALL_BUILD', 'ZERO_CHECK' } | Sort-Object)
    $expected = @($contracts[$name] | Sort-Object)
    if (Compare-Object $actual $expected) { throw "Incorrect $name source-project composition: $($actual -join ', ')." }
    [xml]$xml = Get-Content -LiteralPath $solution -Raw
    if ((@($xml.Solution.Configurations.BuildType.Name) -join ';') -ne 'Debug;Release' -or
        $xml.Solution.Configurations.Platform.Name -ne 'x64') { throw "Wrong $name solution configurations." }
    foreach ($project in $projects) {
        [xml]$vcx = Get-Content -LiteralPath $project.Path -Raw
        foreach ($reference in $vcx.SelectNodes("//*[local-name()='ProjectReference'][@Include]")) {
            $path = [IO.Path]::GetFullPath((Join-Path (Split-Path -Parent $project.Path) $reference.GetAttribute('Include')))
            if (-not $path.StartsWith($directory + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
                throw "Source project reference escapes $name ownership: $path."
            }
        }
    }
    $hash = (Get-FileHash -LiteralPath $solution).Hash
    & cmake --preset "win-$($name.ToLowerInvariant())" -S $repository -B $directory
    if ($LASTEXITCODE -ne 0 -or (Get-FileHash -LiteralPath $solution).Hash -ne $hash) {
        throw "Repeated $name configuration was not deterministic."
    }
    if (Get-ChildItem -LiteralPath $directory -Recurse -Filter '*.slnf') { throw 'Manual workflow generated a solution filter.' }
    Write-Host "$name solution contract passed: $($actual.Count) production projects."
}
$viewerProjects = @(Read-SolutionProjects (Join-Path $root 'viewer/Elf3D-Viewer.slnx'))
$viewer = Get-SolutionStartupProject $viewerProjects
if ($viewer.Name -ne 'elf3d_viewer') { throw 'Viewer startup project is incorrect.' }
[xml]$viewerXml = Get-Content -LiteralPath $viewer.Path -Raw
foreach ($configuration in @('Debug', 'Release')) {
    $node = @($viewerXml.SelectNodes("//*[local-name()='LocalDebuggerWorkingDirectory']") |
        Where-Object { $_.GetAttribute('Condition') -like "*'$configuration|x64'*" })
    $expected = (Join-Path $root "viewer/bin/$configuration").Replace('\', '/')
    if ($node.Count -ne 1 -or $node[0].InnerText.Replace('\', '/') -ne $expected) { throw 'Viewer debugger directory is incorrect.' }
}
if ($viewerXml.SelectNodes("//*[local-name()='LocalDebuggerCommandArguments']").Count) { throw 'Viewer must not require model arguments.' }
if (-not $Build) { Write-Host "Component configure contracts passed; retained at $root."; return }

function Invoke-ComponentBuild {
    param([string]$Component, [string]$Configuration, [string]$Target = '')
    $arguments = @('--build', (Join-Path $root $Component), '--config', $Configuration, '--parallel', '4')
    if ($Target) { $arguments += @('--target', $Target) }
    $log = Join-Path $root "$Component-$Configuration-$Target.log"
    & cmake @arguments *> $log
    if ($LASTEXITCODE -ne 0) { Get-Content -LiteralPath $log -Tail 60; throw "Build failed: $Component $Configuration $Target." }
}
function Get-PchSnapshot {
    param([string]$Component)
    return (@(Get-ChildItem -LiteralPath (Join-Path $root $Component) -Recurse -File -Filter '*.pch' |
        Where-Object { $_.FullName -match '[\\/]Debug[\\/]' } | ForEach-Object {
            "$($_.FullName)|$($_.LastWriteTimeUtc.Ticks)|$((Get-FileHash -LiteralPath $_.FullName).Hash)"
        } | Sort-Object) -join "`n")
}
function Get-UpstreamSnapshot {
    param([string[]]$Components)
    $records = foreach ($component in $Components) {
        foreach ($kind in @('bin', 'lib')) {
            $directory = Join-Path $root "$component/$kind"
            if (-not (Test-Path -LiteralPath $directory)) { continue }
            foreach ($file in Get-ChildItem -LiteralPath $directory -Recurse -File) {
                "$($file.FullName)|$($file.LastWriteTimeUtc.Ticks)|$((Get-FileHash -LiteralPath $file.FullName).Hash)"
            }
        }
    }
    return ($records | Sort-Object) -join "`n"
}
foreach ($configuration in @('Debug', 'Release')) {
    foreach ($component in @('dependencies', 'engine', 'imgui', 'viewer')) {
        Invoke-ComponentBuild $component $configuration
        Write-Host "Built $component $configuration."
    }
    $bin = Join-Path $root "viewer/bin/$configuration"
    & (Join-Path $bin 'elf3d_viewer.exe') --smoke
    if ($LASTEXITCODE -eq 77) { Write-Warning "Graphics SKIPPED: Viewer $configuration (77)." }
    elseif ($LASTEXITCODE -ne 0) { throw "Viewer smoke failed: $configuration." }
    else { Write-Host "Viewer graphics smoke passed: $configuration." }
    $sourceDll = Join-Path $root "engine/bin/$configuration/elf3d.dll"
    if ((Get-FileHash $sourceDll).Hash -ne (Get-FileHash (Join-Path $bin 'elf3d.dll')).Hash) { throw 'Deployed DLL differs.' }
    if ($configuration -eq 'Debug' -and -not (Test-Path (Join-Path $bin 'elf3d.pdb'))) { throw 'Engine Debug PDB is missing.' }
    foreach ($asset in Get-ChildItem -LiteralPath (Join-Path $repository 'apps/viewer/assets') -Recurse -File) {
        $relative = [IO.Path]::GetRelativePath((Join-Path $repository 'apps/viewer/assets'), $asset.FullName)
        if ((Get-FileHash $asset.FullName).Hash -ne (Get-FileHash (Join-Path $bin "assets/$relative")).Hash) { throw "Asset differs: $relative." }
    }
    foreach ($component in @('dependencies', 'engine', 'imgui', 'viewer')) {
        $upstream = switch ($component) { dependencies { @() }; engine { @('dependencies') }; imgui { @('dependencies', 'engine') }; viewer { @('dependencies', 'engine', 'imgui') } }
        $before = Get-UpstreamSnapshot $upstream
        Invoke-ComponentBuild $component $configuration clean
        if ((Get-UpstreamSnapshot $upstream) -ne $before) { throw "$component Clean changed upstream outputs." }
        Invoke-ComponentBuild $component $configuration
        if ((Get-UpstreamSnapshot $upstream) -ne $before) { throw "$component Rebuild changed upstream outputs." }
    }
    Write-Host "Clean/Rebuild ownership passed: $configuration."
}
# A changed DLL must copy even when the executable and import library do not change.
$dll = Join-Path $root 'engine/bin/Debug/elf3d.dll'
$original = [IO.File]::ReadAllBytes($dll)
$exe = Join-Path $root 'viewer/bin/Debug/elf3d_viewer.exe'
$exeTime = (Get-Item $exe).LastWriteTimeUtc.Ticks
try {
    [IO.File]::WriteAllBytes($dll, [byte[]]($original + [byte]0))
    Invoke-ComponentBuild viewer Debug
    if ((Get-FileHash $dll).Hash -ne (Get-FileHash (Join-Path $root 'viewer/bin/Debug/elf3d.dll')).Hash) { throw 'Incremental DLL deployment failed.' }
    if ((Get-Item $exe).LastWriteTimeUtc.Ticks -ne $exeTime) { throw 'DLL deployment unnecessarily relinked Viewer.' }
} finally { [IO.File]::WriteAllBytes($dll, $original) }
Invoke-ComponentBuild viewer Debug

$header = Join-Path $repository 'include/elf3d/core/error.h'
$headerTime = (Get-Item -LiteralPath $header).LastWriteTimeUtc
$headerHash = (Get-FileHash -LiteralPath $header).Hash
$pchBefore = Get-PchSnapshot imgui
$object = Get-ChildItem -LiteralPath (Join-Path $root 'imgui') -Recurse -File -Filter context.obj |
    Where-Object { $_.FullName -match '[\\/]Debug[\\/]' } | Select-Object -First 1
if ($null -eq $object) { throw 'ImGui Debug object is missing.' }
$objectTime = $object.LastWriteTimeUtc.Ticks
try {
    [IO.File]::SetLastWriteTimeUtc($header, [datetime]::UtcNow.AddSeconds(2))
    Invoke-ComponentBuild imgui Debug
    if ((Get-Item -LiteralPath $object.FullName).LastWriteTimeUtc.Ticks -eq $objectTime) {
        throw 'Changed public header did not recompile the downstream integration.'
    }
} finally { [IO.File]::SetLastWriteTimeUtc($header, $headerTime) }
if ((Get-FileHash -LiteralPath $header).Hash -ne $headerHash) { throw 'Public header preservation failed.' }
if ((Get-PchSnapshot imgui) -ne $pchBefore) { throw 'A public-header edit rebuilt the integration PCH.' }
Write-Host 'Public-header incremental rebuild preserved PCH.'
Invoke-ComponentBuild viewer Debug

foreach ($source in @('modules/renderer/src/render_list.cpp', 'modules/renderer/src/renderer_detail.h')) {
    $path = Join-Path $repository $source
    $time = (Get-Item -LiteralPath $path).LastWriteTimeUtc
    $hash = (Get-FileHash -LiteralPath $path).Hash
    $object = Get-ChildItem -LiteralPath (Join-Path $root 'engine') -Recurse -File -Filter render_list.obj |
        Where-Object { $_.FullName -match '[\\/]Debug[\\/]' } | Select-Object -First 1
    if ($null -eq $object) { throw 'Renderer Debug object is missing.' }
    $objectTime = $object.LastWriteTimeUtc.Ticks
    $pchBefore = Get-PchSnapshot engine
    try {
        [IO.File]::SetLastWriteTimeUtc($path, [datetime]::UtcNow.AddSeconds(2))
        Invoke-ComponentBuild engine Debug
        if ((Get-Item -LiteralPath $object.FullName).LastWriteTimeUtc.Ticks -eq $objectTime) {
            throw "Changed $source did not recompile its renderer consumer."
        }
        if ((Get-PchSnapshot engine) -ne $pchBefore) { throw "Changed $source rebuilt a stable PCH." }
    } finally { [IO.File]::SetLastWriteTimeUtc($path, $time) }
    if ((Get-FileHash -LiteralPath $path).Hash -ne $hash) { throw "Incremental source preservation failed: $source." }
    Write-Host "Incremental consumer rebuild and stable PCH passed: $source."
}

$missing = Join-Path $root 'engine/lib/Debug/elf3d.lib'
$saved = "$missing.prerequisite-test"
try {
    Move-Item -LiteralPath $missing -Destination $saved
    $diagnostic = @(& cmake --build (Join-Path $root 'viewer') --config Debug --parallel 4 2>&1) -join "`n"
    if ($LASTEXITCODE -eq 0 -or $diagnostic -notmatch 'Missing prerequisite engine \(Debug\)' -or
        $diagnostic -notmatch 'Elf3D-Engine.slnx' -or -not (Test-Path (Join-Path $root 'engine/lib/Release/elf3d.lib'))) {
        throw "Missing Debug dependency did not produce its own diagnostic: $diagnostic"
    }
    $diagnostic | Set-Content (Join-Path $root 'missing-debug.log')
} finally { if (Test-Path $saved) { Move-Item -LiteralPath $saved -Destination $missing } }

# An isolated CPU client imports only Model's external static closure.
$clientSource = Join-Path $root 'model-client-source'
$clientBuild = Join-Path $root 'model-client'
New-Item -ItemType Directory -Path $clientSource -Force | Out-Null
@'
cmake_minimum_required(VERSION 4.4.3)
project(Elf3DModelClient LANGUAGES C CXX)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
set(ELF3D_BUILD_COMPONENT client)
include("${ELF3D_SOURCE_DIR}/cmake/dependencies.cmake")
include("${ELF3D_SOURCE_DIR}/cmake/local-components.cmake")
if(ELF3D_TEST_TOOLCHAIN_MISMATCH)
    set(CMAKE_CXX_COMPILER_VERSION incompatible-test-toolchain)
endif()
if(ELF3D_TEST_ARCHITECTURE_MISMATCH)
    set(CMAKE_GENERATOR_PLATFORM incompatible-test-architecture)
endif()
if(ELF3D_TEST_SOURCE_ROOT)
    set(ELF3D_SOURCE_ROOT "${ELF3D_TEST_SOURCE_ROOT}")
endif()
elf3d_import_component(dependencies elf3d_third_party_zlib elf3d_third_party_png
    elf3d_third_party_jpeg elf3d_third_party_cgltf elf3d_third_party_mikktspace)
elf3d_import_component(engine elf3d_model)
if(TARGET elf3d OR TARGET glfw OR TARGET elf3d_third_party_glad OR TARGET elf3d_foundation_modules)
    message(FATAL_ERROR "CPU client imported graphics or internal modules")
endif()
add_executable(model_client main.cpp)
target_link_libraries(model_client PRIVATE elf3d::model)
'@ | Set-Content (Join-Path $clientSource 'CMakeLists.txt')
@'
#include <elf3d/model.h>
int main(int argc, char** argv)
{
    if (argc != 2) { return 2; }
    auto loaded = elf3d::load_document(argv[1]);
    return loaded && loaded.value().document.primitive_count() != 0 ? 0 : 1;
}
'@ | Set-Content (Join-Path $clientSource 'main.cpp')
& cmake -S $clientSource -B $clientBuild -G 'Visual Studio 18 2026' -A x64 -T v145,host=x64 `
    '-DCMAKE_CONFIGURATION_TYPES=Debug;Release' "-DELF3D_SOURCE_DIR=$repository" "-DELF3D_COMPONENT_ROOT=$root"
if ($LASTEXITCODE -ne 0) { throw 'CPU imported Model client configure failed.' }
foreach ($configuration in @('Debug', 'Release')) {
    & cmake --build $clientBuild --config $configuration --parallel 4
    if ($LASTEXITCODE -ne 0) { throw 'CPU imported Model client build failed.' }
    & (Join-Path $clientBuild "$configuration/model_client.exe") (Join-Path $repository 'tests/fixtures/elf3d_smoke/elf3d_smoke.gltf')
    if ($LASTEXITCODE -ne 0) { throw 'CPU imported Model client execution failed.' }
}
$shadow = Join-Path $root 'other-source'
New-Item -ItemType Directory -Path (Join-Path $shadow 'cmake') -Force | Out-Null
foreach ($file in @('CMakeLists.txt', 'cmake/dependencies.cmake', 'cmake/target-interfaces.cmake')) {
    Copy-Item -LiteralPath (Join-Path $repository $file) -Destination (Join-Path $shadow $file)
}
$mixed = Join-Path $root 'mixed-root'
New-Item -ItemType Directory -Path (Join-Path $mixed 'dependencies') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'dependencies/local') -Destination (Join-Path $mixed 'dependencies/local') -Recurse -Force
foreach ($case in @(
    @{Name='toolchain'; Enable='-DELF3D_TEST_TOOLCHAIN_MISMATCH=ON'; Restore='-DELF3D_TEST_TOOLCHAIN_MISMATCH=OFF'},
    @{Name='architecture'; Enable='-DELF3D_TEST_ARCHITECTURE_MISMATCH=ON'; Restore='-DELF3D_TEST_ARCHITECTURE_MISMATCH=OFF'},
    @{Name='source-root'; Enable="-DELF3D_TEST_SOURCE_ROOT=$shadow"; Restore='-DELF3D_TEST_SOURCE_ROOT='},
    @{Name='component-root'; Enable="-DELF3D_COMPONENT_ROOT=$mixed"; Restore="-DELF3D_COMPONENT_ROOT=$root"}
)) {
    $argument = $case.Enable
    $diagnostic = @(& cmake -S $clientSource -B $clientBuild $argument 2>&1) -join "`n"
    if ($LASTEXITCODE -eq 0 -or $diagnostic -notmatch 'Incompatible dependencies metadata') {
        throw "Mixed $($case.Name) was not rejected: $diagnostic"
    }
    $diagnostic | Set-Content (Join-Path $root "incompatible-$($case.Name).log")
    $argument = $case.Restore
    & cmake -S $clientSource -B $clientBuild $argument
    if ($LASTEXITCODE -ne 0) { throw 'CPU client restore failed.' }
}
Write-Host "Component build, graphics, ownership, deployment, configuration isolation and imported Model contracts passed. Evidence: $root"
