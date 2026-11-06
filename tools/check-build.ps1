#Requires -Version 7.6.5
[CmdletBinding()]
param([string]$RepositoryRoot=(Split-Path -Parent $PSScriptRoot),[switch]$CheckFixtures)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'

function Test-NativeXml([string]$Text) {
    if ($Text -match '(?i)\b[A-Z]:[\\/]') { 'absolute-path' }
    if ($Text -match '(?i)\b(?:cmake|ctest|cpack)(?:\.exe)?(?:\s|&quot;|")') { 'build-generator' }
    if ($Text -match '(?i)<(?:ScanSourceForModuleDependencies|EnableModules|BuildStlModules)>(?!\s*(?:false|off|0)\s*</)[^<]+</|<CompileAs>\s*CompileAsCppModule\s*</|/(?:interface|ifcOutput|headerUnit|exportHeader|reference)\b') { 'cpp-modules' }
}
function Test-ProjectFileItems([xml]$Project,[string]$ProjectDirectory) {
    $fileTypes=@('ClCompile','ClInclude','ResourceCompile','None','Content','Image','Text',
        'CustomBuild','Natvis','Midl','FxCompile','MASM','Object','ProjectReference')
    $seen=[Collections.Generic.Dictionary[string,string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($item in $Project.SelectNodes("//*[local-name()='ItemGroup']/*[@Include]")) {
        if ($item.LocalName -notin $fileTypes) { continue }
        foreach ($include in $item.GetAttribute('Include').Split(';',[StringSplitOptions]::RemoveEmptyEntries)) {
            # This static check covers declared literal files, not MSBuild expansion.
            if ($include -match '\$\(|@\(|%\(|[*?]') { continue }
            $path=[IO.Path]::GetFullPath($include.Trim().Replace('/',[IO.Path]::DirectorySeparatorChar),$ProjectDirectory)
            $first=''
            if ($seen.TryGetValue($path,[ref]$first)) {
                "Duplicate project item: $path ($first and $($item.LocalName))"
            } else { $seen.Add($path,$item.LocalName) }
        }
    }
}
function Test-ProjectFileItemFixtures {
    $directory=[IO.Path]::GetFullPath((Join-Path $RepositoryRoot 'win/projects'))
    $valid=@(
        '<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003"><ItemGroup><ClCompile Include="../../src/sample/a.cpp"/><ClInclude Include="../../src/sample/a.h"/></ItemGroup></Project>',
        '<Project><ItemGroup><ClInclude Include="a.h"/><ClInclude Update="a.h"/><ClInclude Remove="a.h"/></ItemGroup><ItemDefinitionGroup><ClInclude Include="a.h"/></ItemDefinitionGroup></Project>',
        '<Project><ItemGroup><Elf3DLibrary Include="same.lib"/><Elf3DPrerequisite Include="same.lib"/><Content Include="assets/**/*"/></ItemGroup></Project>')
    foreach ($sample in $valid) {
        if (@(Test-ProjectFileItems ([xml]$sample) $directory).Count) { throw 'Valid project-item fixture was rejected.' }
    }
    $negative=@(
        '<ItemGroup><ClInclude Include="a.h"/><ClInclude Include="a.h"/></ItemGroup>',
        '<ItemGroup><ClInclude Include="a.h"/><ClCompile Include="a.h"/></ItemGroup>',
        '<ItemGroup><ClInclude Include="../../src/sample/A.h"/><ClInclude Include="..\..\src\sample\a.h"/></ItemGroup>',
        '<ItemGroup><ClInclude Include="./a.h"/><ClInclude Include="folder/../a.h"/></ItemGroup>',
        '<ItemGroup><ClInclude Include="a.h"/></ItemGroup><ItemGroup><None Include="a.h"/></ItemGroup>',
        '<ItemGroup><ClInclude Include="a.h;b.h"/><ClInclude Include="b.h"/></ItemGroup>',
        '<ItemGroup><ResourceCompile Include="a.rc"/><ResourceCompile Include="a.rc"/></ItemGroup>',
        '<ItemGroup><ProjectReference Include="a.vcxproj"/><ProjectReference Include="./a.vcxproj"/></ItemGroup>')
    foreach ($sample in $negative) {
        $issues=@(Test-ProjectFileItems ([xml]("<Project>$sample</Project>")) $directory)
        if ($issues.Count -ne 1 -or $issues[0] -notlike 'Duplicate project item:*') { throw "Invalid project-item fixture was accepted: $sample" }
    }
    Write-Host "Project-item fixtures: $($valid.Count) valid accepted, $($negative.Count) duplicates rejected."
}
$failures=[Collections.Generic.List[string]]::new()
$parts=@('Dependencies','Engine','ImGui','Viewer','Cmd')
$productionOwners=@{}
$projectRoot=Join-Path $RepositoryRoot 'win/projects'
foreach ($part in $parts) {
    $solution=Join-Path $RepositoryRoot "win/Elf3D-$part.slnx"
    [xml]$xml=Get-Content -LiteralPath $solution -Raw
    foreach ($entry in $xml.Solution.Project) {
        $path=[IO.Path]::GetFullPath((Join-Path (Split-Path $solution) $entry.Path))
        [xml]$project=Get-Content -LiteralPath $path -Raw
        $globals=$project.SelectSingleNode("//*[local-name()='PropertyGroup' and @Label='Globals']")
        if ($globals.Elf3DPart -ne $part -or $globals.Elf3DOutputKind -ne 'Product') { $failures.Add("Wrong production membership: $path") }
        foreach ($item in $project.SelectNodes("//*[local-name()='ItemGroup']/*[local-name()='ClCompile' and @Include]")) {
            $source=[IO.Path]::GetFullPath((Join-Path (Split-Path $path) $item.GetAttribute('Include')))
            if (!(Test-Path -LiteralPath $source -PathType Leaf)) { $failures.Add("Missing source: $source") }
            if ($productionOwners.ContainsKey($source)) { $failures.Add("Duplicate compiling owner: $source") }
            $productionOwners[$source]=$path
        }
    }
}
foreach ($file in Get-ChildItem -LiteralPath (Join-Path $RepositoryRoot 'win') -Recurse -File | Where-Object Extension -in '.vcxproj','.props','.targets','.slnx') {
    $text=Get-Content -LiteralPath $file.FullName -Raw
    foreach ($issue in @(Test-NativeXml $text)) { $failures.Add("$issue in $($file.FullName)") }
    if ($file.Extension -eq '.vcxproj') {
        [xml]$project=$text
        foreach ($issue in @(Test-ProjectFileItems $project $file.DirectoryName)) {
            $failures.Add("$issue in $($file.FullName)")
        }
        foreach ($reference in $project.SelectNodes("//*[local-name()='ProjectReference']")) {
            $destination=[IO.Path]::GetFullPath((Join-Path $file.DirectoryName $reference.GetAttribute('Include')))
            if (!(Test-Path -LiteralPath $destination)) { $failures.Add("Missing project reference: $destination") }
            [xml]$target=Get-Content -LiteralPath $destination -Raw
            $fromPart=$project.SelectSingleNode("//*[local-name()='Elf3DPart']").InnerText
            $toPart=$target.SelectSingleNode("//*[local-name()='Elf3DPart']").InnerText
            if ($fromPart -ne $toPart) { $failures.Add("Cross-component build traversal: $($file.Name) -> $($reference.Include)") }
        }
    }
}
$tests=@(Get-Content -LiteralPath (Join-Path $RepositoryRoot 'tests/tests.json') -Raw | ConvertFrom-Json)
if (@($tests | Group-Object name | Where-Object Count -ne 1).Count) { $failures.Add('Duplicate test scenario names.') }
foreach ($test in $tests) {
    if ($test.timeout -le 0 -or !$test.profiles.Count -or !$test.configurations.Count) { $failures.Add("Invalid test execution contract: $($test.name)") }
}
if ($CheckFixtures) {
    Test-ProjectFileItemFixtures
    foreach ($sample in @('<Project><ScanSourceForModuleDependencies>false</ScanSourceForModuleDependencies></Project>', '<Project><EnableModules> false </EnableModules></Project>')) {
        if (@(Test-NativeXml $sample).Count) { throw 'Valid native-build fixture was rejected.' }
    }
    $negative=@('<Project><Command>cmake --build build</Command></Project>', '<Project><Command>ctest.exe --test-dir build</Command></Project>',
        '<Project><ClCompile Include="Q:\elsewhere\file.cpp" /></Project>', '<Project><EnableModules>true</EnableModules></Project>',
        '<Project><ScanSourceForModuleDependencies>true</ScanSourceForModuleDependencies></Project>', '<Project><BuildStlModules>true</BuildStlModules></Project>',
        '<Project><CompileAs>CompileAsCppModule</CompileAs></Project>', '<Project><EnableModules>$(ExperimentalModules)</EnableModules></Project>')
    foreach ($sample in $negative) { if (!@(Test-NativeXml $sample).Count) { throw "Invalid native-build fixture was accepted: $sample" } }
    Write-Host "Native build fixtures: 2 valid accepted, $($negative.Count) invalid rejected."
}
if ($failures.Count) { throw ($failures -join "`n") }
Write-Host "Native build contracts passed: five production parts, $($productionOwners.Count) unique source owners, $($tests.Count) scenarios."
