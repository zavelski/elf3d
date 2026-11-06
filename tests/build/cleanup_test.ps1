[CmdletBinding()]
param([Parameter(Mandatory)][string]$RepositoryRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $RepositoryRoot 'tools/storage.ps1')
$clean=Join-Path $RepositoryRoot 'tools/clean.ps1'
$sandbox=[IO.Path]::GetFullPath((Join-Path $RepositoryRoot 'out/tmp/cleanup-proof'))
$allowed=[IO.Path]::GetFullPath((Join-Path $RepositoryRoot 'out/tmp'))+'\'
if (!$sandbox.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid cleanup fixture path.' }
if (Test-Path -LiteralPath $sandbox) { throw 'Previous cleanup fixture evidence exists.' }
$preserved=@('x64/product.dll','out/symbols/Debug/product.pdb','out/release/old.zip','out/release/new.zip',
    'out/reference/baseline.bin','out/validation/reference/baseline.bin','win/.vs/personal.suo','tests/source.cpp','src/source.cpp',
    'out/reports/tests/Full-Debug-failure.json','out/audits/state.json','out/render-quality/reference.png','out/unknown/old.bin')
$disposable=@('out/tmp/scratch.txt','out/temp/scratch.txt','out/public/source/file.txt','out/validation/native/obj.txt',
    'out/tests/Debug/test.exe','out/tools/Debug/tool.exe','out/obj/Debug/project/file.obj')
function Assert-Exists([string[]]$Paths) {
    foreach ($relative in $Paths) { if (!(Test-Path -LiteralPath (Join-Path $sandbox $relative))) { throw "Cleanup destroyed preserved $relative" } }
}
function Assert-Rejected([scriptblock]$Action,[string]$Pattern) {
    $rejected=$false
    try { & $Action } catch {
        if ($_.Exception.Message -notmatch $Pattern) { throw }
        $rejected=$true
    }
    if (!$rejected) { throw "Expected cleanup rejection: $Pattern" }
}
function Write-Fixture([string]$Relative,[string]$Text='fixture') {
    $path=Join-Path $sandbox $Relative
    New-Item -ItemType Directory -Path (Split-Path $path) -Force | Out-Null
    [IO.File]::WriteAllText($path,$Text)
}
try {
    foreach ($relative in $preserved+$disposable) { Write-Fixture $relative }
    Write-Fixture 'out/tests/Debug/run/old.glb'
    Write-Fixture 'out/release/elf3d-viewer-1.0.0-windows-x64/stage.dll'
    Write-Fixture 'out/release/elf3d-viewer-1.0.0-windows-x64.zip'
    $oldPaths=@('out/tmp','out/tests/Debug/run','out/release/elf3d-viewer-1.0.0-windows-x64')
    foreach ($relative in $oldPaths) {
        $path=Join-Path $sandbox $relative
        foreach ($item in @(Get-ChildItem -LiteralPath $path -Recurse -Force)+@(Get-Item -LiteralPath $path)) {
            $item.LastWriteTimeUtc=[datetime]::UtcNow.AddDays(-10)
        }
    }
    & $clean -RepositoryRoot $sandbox -WhatIf
    Assert-Exists ($preserved+$disposable+@('out/tests/Debug/run/old.glb','out/release/elf3d-viewer-1.0.0-windows-x64/stage.dll'))
    & $clean -RepositoryRoot $sandbox -OlderThanDays 14
    Assert-Exists ($preserved+$disposable+@('out/tests/Debug/run/old.glb','out/release/elf3d-viewer-1.0.0-windows-x64/stage.dll'))
    # The newest descendant, not the directory's timestamp, retains recent failures.
    (Get-Item -LiteralPath (Join-Path $sandbox 'out/temp')).LastWriteTimeUtc=[datetime]::UtcNow.AddDays(-20)
    & $clean -RepositoryRoot $sandbox
    Assert-Exists ($preserved+@('out/temp/scratch.txt','out/public/source/file.txt','out/validation/native/obj.txt',
        'out/tests/Debug/test.exe','out/tools/Debug/tool.exe','out/obj/Debug/project/file.obj','out/release/elf3d-viewer-1.0.0-windows-x64.zip'))
    foreach ($relative in $oldPaths) { if (Test-Path -LiteralPath (Join-Path $sandbox $relative)) { throw "Routine retained stale $relative" } }
    foreach ($relative in $disposable) { Write-Fixture $relative }
    & $clean -RepositoryRoot $sandbox -Scope All -WhatIf
    Assert-Exists ($preserved+$disposable)
    & $clean -RepositoryRoot $sandbox -Scope All
    Assert-Exists $preserved
    foreach ($relative in $disposable) { if (Test-Path -LiteralPath (Join-Path $sandbox $relative)) { throw "Cleanup retained selected $relative" } }
    (Get-Item -LiteralPath (Join-Path $sandbox 'out/release/old.zip')).LastWriteTimeUtc=[datetime]'2020-01-01'
    (Get-Item -LiteralPath (Join-Path $sandbox 'out/release/elf3d-viewer-1.0.0-windows-x64.zip')).LastWriteTimeUtc=[datetime]'2021-01-01'
    & $clean -RepositoryRoot $sandbox -Scope Releases -Keep 1
    if (!(Test-Path -LiteralPath (Join-Path $sandbox 'out/release/new.zip')) -or (Test-Path -LiteralPath (Join-Path $sandbox 'out/release/old.zip'))) { throw 'Release retention failed.' }
    $out=Join-Path $sandbox 'out'
    Assert-Rejected { Get-Elf3DGeneratedData -AllowedRoot $out -LiteralPath $sandbox } 'escapes'
    Assert-Rejected { Get-Elf3DGeneratedData -AllowedRoot $out -LiteralPath $out } 'escapes'
    Write-Fixture 'out/tmp/keep.txt'
    Write-Fixture 'out/temp/nested/.git' 'gitdir: fixture'
    Assert-Rejected { & $clean -RepositoryRoot $sandbox -Scope Temp } 'Git repository'
    Assert-Exists @('out/tmp/keep.txt','out/temp/nested/.git')
    Remove-Item -LiteralPath (Join-Path $out 'temp/nested/.git')
    Write-Fixture 'out/temp/nested/HEAD' 'ref: refs/heads/main'
    New-Item -ItemType Directory -Path (Join-Path $out 'temp/nested/objects') -Force | Out-Null
    Assert-Rejected { & $clean -RepositoryRoot $sandbox -Scope Temp } 'Git repository'
    Remove-Item -LiteralPath (Join-Path $out 'temp/nested') -Recurse -Force
    $junction=Join-Path $out 'temp/link'
    try {
        New-Item -ItemType Junction -Path $junction -Target (Join-Path $out 'reference') | Out-Null
        Assert-Rejected { & $clean -RepositoryRoot $sandbox -Scope Temp } 'reparse point'
        Assert-Rejected { Get-Elf3DGeneratedData -AllowedRoot $out -LiteralPath (Join-Path $junction 'missing') } 'reparse point'
        Assert-Exists @('out/tmp/keep.txt','out/reference/baseline.bin')
    } finally {
        if (Test-Path -LiteralPath $junction) { Remove-Item -LiteralPath $junction -Force }
    }
    Write-Host 'PASS cleanup: Routine retention/cache reuse, WhatIf, deep scopes, release retention, preflight, path/repository/junction guards.'
} finally {
    if (Test-Path -LiteralPath $sandbox) { Remove-Item -LiteralPath $sandbox -Recurse -Force }
}
