#Requires -Version 7.6.5
[CmdletBinding()]
param([Parameter(Mandatory)][string]$RepositoryRoot, [Parameter(Mandatory)][string]$WorkRoot,
    [Parameter(Mandatory)][string]$ProductRoot, [Parameter(Mandatory)][string]$Configuration,
    [Parameter(Mandatory)][string]$Profile, [Parameter(Mandatory)][string]$Compiler)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath($RepositoryRoot).TrimEnd('\','/')
$stateRoot=Join-Path $WorkRoot 'state'
New-Item -ItemType Directory -Path $stateRoot -Force | Out-Null
$sourceFile=Join-Path $root 'out/state/source.json'
$sourceTree='working-tree'
if (Test-Path -LiteralPath $sourceFile) {
    $source=Get-Content -LiteralPath $sourceFile -Raw | ConvertFrom-Json
    $sourceTree=$source.SourceTree
    $relativeFiles=@($source.Files.PSObject.Properties.Name)
} else {
    $previousIndex=[Environment]::GetEnvironmentVariable('GIT_INDEX_FILE','Process')
    try {
        if (Get-Command git -ErrorAction SilentlyContinue) {
            $gitRoot=(& git -C $root rev-parse --show-toplevel 2>$null)
            if ($LASTEXITCODE -eq 0 -and [IO.Path]::GetFullPath($gitRoot).TrimEnd('\','/') -eq $root) {
                $env:GIT_INDEX_FILE=Join-Path $stateRoot 'build.index'
                git -C $root read-tree HEAD
                if ($LASTEXITCODE) { throw 'Cannot initialize build provenance index.' }
                git -C $root add -A 2> (Join-Path $stateRoot 'build-index.log')
                if ($LASTEXITCODE) { throw 'Cannot snapshot build source.' }
                $sourceTree=(& git -C $root write-tree).Trim()
                if ($LASTEXITCODE) { throw 'Cannot record build source tree.' }
            }
        }
    } finally {
        if ($null -eq $previousIndex) { Remove-Item Env:GIT_INDEX_FILE -ErrorAction SilentlyContinue }
        else { $env:GIT_INDEX_FILE=$previousIndex }
    }
    $relativeFiles=@('VERSION')
    foreach ($directory in 'src','include','win','third_party') {
        $relativeFiles+=@(Get-ChildItem -LiteralPath (Join-Path $root $directory) -Recurse -File |
            Where-Object { $_.FullName -notmatch '[\\/]\.vs[\\/]' -and $_.Extension -notin '.user','.suo' } |
            ForEach-Object { [IO.Path]::GetRelativePath($root,$_.FullName).Replace('\','/') })
    }
}
$inputs=[ordered]@{}
foreach ($relative in @($relativeFiles | Sort-Object -Unique)) {
    $path=[IO.Path]::GetFullPath((Join-Path $root $relative))
    if (!$path.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)) { throw "Source record escapes source root: $relative" }
    $hash=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ((Test-Path -LiteralPath $sourceFile) -and $hash -ne $source.Files.$relative) { throw "Audited source changed during build: $relative" }
    $inputs[$relative]=$hash
}
$outputs=[ordered]@{}
foreach ($file in Get-ChildItem -LiteralPath $ProductRoot -File -Recurse) {
    $relative=[IO.Path]::GetRelativePath($ProductRoot,$file.FullName).Replace('\','/')
    if ($file.Extension -in '.exe','.dll','.lib') {
        $debugName=$file.BaseName.EndsWith('D',[StringComparison]::Ordinal)
        if (($Configuration -eq 'Debug') -ne $debugName) { continue }
    }
    $outputs[$relative]=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
}
$record=[ordered]@{Schema=1;SourceRoot=$root;SourceTree=$sourceTree;Version=(Get-Content (Join-Path $root 'VERSION') -Raw).Trim();
    Configuration=$Configuration;Profile=$Profile;Compiler=$Compiler;Platform='x64';ProductRoot=[IO.Path]::GetFullPath($ProductRoot);
    Inputs=$inputs;Outputs=$outputs}
$record | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $stateRoot "build-$Configuration.json") -Encoding utf8
