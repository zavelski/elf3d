#Requires -Version 7.6.5
[CmdletBinding()]
param(
    [ValidateSet('Model','Full')][string]$Profile='Full',
    [ValidateSet('Debug','Release')][string]$Configuration='Debug',
    [string]$RepositoryRoot=(Split-Path -Parent $PSScriptRoot), [string]$WorkRoot=''
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'windows.ps1')
if (!$WorkRoot) { $WorkRoot=Join-Path $RepositoryRoot 'out' }
$headerRoot=Join-Path $WorkRoot "obj/$Configuration/header-contracts-$Profile"
New-Item -ItemType Directory -Path $headerRoot -Force | Out-Null
$public=@(Get-ChildItem -LiteralPath (Join-Path $RepositoryRoot 'include/elf3d') -Recurse -Filter '*.h')
if ($Profile -eq 'Model') {
    $public=@($public | Where-Object { [IO.Path]::GetRelativePath((Join-Path $RepositoryRoot 'include/elf3d'),$_.FullName) -notmatch '^(app|imgui)[\\/]' })
}
$graphPath=Join-Path $RepositoryRoot '.internal/architecture/module-graph.json'
$internal=@()
if (Test-Path -LiteralPath $graphPath) {
    $graph=Get-Content -LiteralPath $graphPath -Raw | ConvertFrom-Json
    $modules=@($graph.modules | Where-Object { $Profile -eq 'Full' -or $_.name -in 'elf.core','elf.math','elf.model','elf.image','elf.gltf' })
    foreach ($module in $modules) {
        foreach ($header in $module.headers) { $internal+=Get-Item -LiteralPath (Join-Path $RepositoryRoot $header) }
    }
} else {
    foreach ($name in @('elf3d_model')+$(if($Profile -eq 'Full'){@('elf3d_runtime')}else{@()})) {
        $project=Join-Path $RepositoryRoot "win/projects/$name.vcxproj"
        [xml]$xml=Get-Content -LiteralPath $project -Raw
        foreach ($item in $xml.SelectNodes("//*[local-name()='ClInclude' and *[local-name()='Elf3DContract' and text()='true']]")) {
            $internal+=Get-Item -LiteralPath (Join-Path (Split-Path $project) $item.GetAttribute('Include'))
        }
    }
}
$headers=@(@($public)+@($internal) | Sort-Object FullName -Unique)
if (!$headers.Count) { throw 'No header contracts found.' }
$combined=''
$sources=@()
foreach ($header in $headers) {
    $relative=[IO.Path]::GetRelativePath($RepositoryRoot,$header.FullName).Replace('\','/')
    $name=$relative -replace '[^a-zA-Z0-9_]','_'
    $include=$header.FullName.Replace('\','/')
    $text="#include `"$include`"`n#include `"$include`"`n"
    $source=Join-Path $headerRoot "$name.cpp"
    Set-Content -LiteralPath $source -Value $text -Encoding utf8
    $sources+=$source
    $combined+=$text
}
$first=Join-Path $headerRoot 'combined.cpp'
$second=Join-Path $headerRoot 'odr.cpp'
Set-Content -LiteralPath $first -Value ($combined+"int main() { return 0; }`n") -Encoding utf8
Set-Content -LiteralPath $second -Value ($combined+"void elf3d_header_odr_probe() {}`n") -Encoding utf8
$sources+=@($first,$second)
$toolchain=Get-Elf3DToolchain
$environment=@{}
foreach ($name in 'PATH','INCLUDE','LIB','LIBPATH') { $environment[$name]=[Environment]::GetEnvironmentVariable($name,'Process') }
try {
    Import-Elf3DCompilerEnvironment $toolchain
    $compiler=Join-Path $toolchain.Tools 'cl.exe'
    $runtime=if($Configuration -eq 'Debug'){'/MDd'}else{'/MD'}
    $arguments=@('/nologo','/std:c++20','/permissive-','/Zc:__cplusplus','/Zc:preprocessor','/W4','/WX','/EHsc',$runtime,'/c','/MP4',"/I$RepositoryRoot/include","/I$RepositoryRoot/src",
        '/external:W0',"/external:I$RepositoryRoot/third_party/glm","/external:I$RepositoryRoot/third_party/imgui","/external:I$RepositoryRoot/third_party/glfw/include","/Fo$headerRoot\")
    if ($Profile -eq 'Full') { $arguments+="/external:I$RepositoryRoot/third_party/glad/include" }
    $arguments+=$sources
    & $compiler @arguments
    if ($LASTEXITCODE) { throw 'Header first/repeated include compilation failed.' }
    & $compiler /nologo $runtime (Join-Path $headerRoot 'combined.obj') (Join-Path $headerRoot 'odr.obj') "/Fe$headerRoot/odr.exe" /link "/PDB:$headerRoot/odr.pdb"
    if ($LASTEXITCODE) { throw 'Header ODR link check failed.' }
} finally {
    foreach ($name in $environment.Keys) { [Environment]::SetEnvironmentVariable($name,$environment[$name],'Process') }
}
Write-Host "PASS $($headers.Count) header contracts without PCH and ODR [$Profile/$Configuration]"
