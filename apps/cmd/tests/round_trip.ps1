#Requires -Version 7.6.5
[CmdletBinding()]
param([Parameter(Mandatory)][string]$Executable, [Parameter(Mandatory)][string]$Verifier,
    [Parameter(Mandatory)][string]$WorkRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$root = [IO.Path]::GetFullPath((Join-Path $WorkRoot ([guid]::NewGuid().ToString('N'))))
New-Item -ItemType Directory -Force -Path $root | Out-Null

function Invoke-Cmd {
    param([string[]]$Tokens, [int]$Expected = 0, [string]$Diagnostic = '')
    $stdout = Join-Path $root 'stdout.txt'
    $stderr = Join-Path $root 'stderr.txt'
    & $Executable @Tokens > $stdout 2> $stderr
    if ($LASTEXITCODE -ne $Expected) { throw "Unexpected exit $LASTEXITCODE (expected $Expected): $(Get-Content $stderr -Raw)" }
    $text = Get-Content $stderr -Raw
    if ($Diagnostic -and $text -notmatch $Diagnostic) { throw "Missing diagnostic '$Diagnostic': $text" }
    if ($Expected -ne 0 -and (Get-Content $stdout -Raw) -match 'Saved ') { throw 'Failure printed success.' }
}

Invoke-Cmd @('--help')
Invoke-Cmd @() 2
$validationLog = Join-Path $root 'validation.txt'
& $Verifier validation 2> $validationLog
if ($LASTEXITCODE -ne 1 -or
    @(Select-String -LiteralPath $validationLog -Pattern 'error: validate \[invalid_reference\]').Count -ne 1) {
    throw 'Validation failure must report its stage, severity, and code exactly once.'
}
foreach ($extension in @('gltf', 'glb')) {
    $input = Join-Path $root "source.$extension"
    & $Verifier fixture $input
    if ($LASTEXITCODE -ne 0) { throw 'Fixture generation failed.' }
    if ($extension -eq 'gltf') {
        $json = Get-Content $input -Raw | ConvertFrom-Json
        $json | Add-Member -NotePropertyName extras -NotePropertyValue @{owner='cmd-test'}
        $json | ConvertTo-Json -Depth 100 | Set-Content $input -Encoding utf8
    }
    foreach ($target in @('gltf', 'glb')) {
        $output = Join-Path $root "rotate-$extension.$target"
        Invoke-Cmd @('--input', $input, '--output', $output, '--rotate', '0', '0', '90')
        if ((Get-Content (Join-Path $root 'stderr.txt') -Raw) -notmatch 'load.*generated_normals') {
            throw 'Import warning was not reported.'
        }
        if ((Get-Content (Join-Path $root 'stderr.txt') -Raw) -match 'metadata_dropped') {
            throw 'Rotation invalidated metadata.'
        }
        & $Verifier verify $input $output rotate
        if ($LASTEXITCODE -ne 0) { throw "Round trip failed: $extension -> $target ($LASTEXITCODE)." }
    }
    $zero = Join-Path $root "zero-$extension.glb"
    Invoke-Cmd @('--input', $input, '--output', $zero, '--rotate', '0', '0', '0')
    if ((Get-Content (Join-Path $root 'stderr.txt') -Raw) -match 'metadata_dropped') { throw 'Zero rotation invalidated metadata.' }
    & $Verifier verify $input $zero zero
    if ($LASTEXITCODE -ne 0) { throw 'Zero rotation round trip failed.' }
    $turns = Join-Path $root "turns-$extension.glb"
    Invoke-Cmd @('--input', $input, '--output', $turns, '--rotate', '360', '-720', '1080')
    if ((Get-Content (Join-Path $root 'stderr.txt') -Raw) -match 'metadata_dropped') { throw 'Whole turns invalidated metadata.' }
    & $Verifier verify $input $turns zero
    if ($LASTEXITCODE -ne 0) { throw 'Whole-turn no-op round trip failed.' }
    Invoke-Cmd @('--input', $input, '--output', $input, '--rotate', '0', '0', '0') 2 'distinct'
    Invoke-Cmd @('--input', $input, '--output', (Join-Path $root "./source.$extension"), '--rotate', '0', '0', '0') 2 'distinct'
    if ($extension -eq 'gltf') {
        $json = Get-Content $input -Raw | ConvertFrom-Json
        $json | Add-Member -NotePropertyName scene -NotePropertyValue 1
        $json | ConvertTo-Json -Depth 100 | Set-Content $input -Encoding utf8
        $authored = Join-Path $root 'authored-default.glb'
        Invoke-Cmd @('--input', $input, '--output', $authored, '--rotate', '0', '0', '90')
        & $Verifier verify $input $authored rotate
        if ($LASTEXITCODE -ne 0) { throw 'Authored default scene round trip failed.' }
    }
}
$source = Join-Path $root 'source.glb'
$missing = Join-Path $root 'absent.glb'
$output = Join-Path $root 'failure.glb'
Invoke-Cmd @('--input', $missing, '--output', $output, '--rotate', '0', '0', '0') 1 'load'
Set-Content $missing 'malformed'
Invoke-Cmd @('--input', $missing, '--output', $output, '--rotate', '0', '0', '0') 1 'load'
Invoke-Cmd @('--input', $source, '--output', (Join-Path $root 'absent/result.glb'), '--rotate', '0', '0', '0') 1 'save'
Invoke-Cmd @('--input', $source, '--output', $output, '--rotate', 'nan', '0', '0') 2
if (Test-Path $output) { throw 'Failed processing wrote an output.' }

# A conflicting .bin directory makes export fail before publishing the main file.
$preserved = Join-Path $root 'preserved.gltf'
Set-Content $preserved 'original-output'
New-Item -ItemType Directory -Path (Join-Path $root 'preserved.bin') | Out-Null
Invoke-Cmd @('--input', $source, '--output', $preserved, '--rotate', '0', '0', '0') 1 'save'
if ((Get-Content $preserved -Raw).Trim() -ne 'original-output') { throw 'Failed export replaced existing output.' }

# Failure while backing up a locked final image must roll back every artifact.
$locked = Join-Path $root 'locked.gltf'
Invoke-Cmd @('--input', $source, '--output', $locked, '--rotate', '0', '0', '0')
$artifacts = @(Get-ChildItem -LiteralPath $root -File -Filter 'locked.*')
$before = @($artifacts | ForEach-Object { (Get-FileHash -LiteralPath $_.FullName).Hash })
$lockedImage = @($artifacts | Where-Object Extension -EQ '.png')[0].FullName
$lock = [IO.File]::Open($lockedImage, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
try { Invoke-Cmd @('--input', $source, '--output', $locked, '--rotate', '0', '0', '90') 1 'save' }
finally { $lock.Dispose() }
$after = @($artifacts | ForEach-Object { (Get-FileHash -LiteralPath $_.FullName).Hash })
if (($before -join ';') -ne ($after -join ';')) { throw 'Export rollback changed existing destination artifacts.' }

$alias = Join-Path $root 'alias.glb'
New-Item -ItemType HardLink -Path $alias -Target $source | Out-Null
Invoke-Cmd @('--input', $source, '--output', $alias, '--rotate', '0', '0', '0') 2 'distinct'

# The test verifier uses narrow arguments, so validate Unicode output via an ASCII copy.
$unicode = Join-Path $root 'model with spaces 🌍.glb'
Copy-Item -LiteralPath $source -Destination $unicode
$unicodeOutput = Join-Path $root 'result 🌍.glb'
Invoke-Cmd @('--input', $unicode, '--output', $unicodeOutput, '--rotate', '0', '0', '90')
$copy = Join-Path $root 'unicode-result.glb'
Copy-Item -LiteralPath $unicodeOutput -Destination $copy
& $Verifier verify $source $copy rotate
if ($LASTEXITCODE -ne 0) { throw 'Unicode path processing failed.' }
$unicodeJson = Join-Path $root 'text model 🌍.gltf'
Invoke-Cmd @('--input', $unicode, '--output', $unicodeJson, '--rotate', '0', '0', '90')
$unicodeReloaded = Join-Path $root 'unicode-reloaded.glb'
Invoke-Cmd @('--input', $unicodeJson, '--output', $unicodeReloaded, '--rotate', '0', '0', '0')
& $Verifier verify $source $unicodeReloaded rotate
if ($LASTEXITCODE -ne 0) { throw 'Unicode glTF sidecar round trip failed.' }

# Rotating a distant camera can overflow its finite float matrix; do not publish output.
$overflowInput = Join-Path $root 'overflow.gltf'
$overflow = Get-Content (Join-Path $root 'source.gltf') -Raw | ConvertFrom-Json
if (-not $overflow.nodes[3].PSObject.Properties['matrix']) {
    # Omitted glTF matrices mean identity; make it explicit before editing the fixture.
    $overflow.nodes[3] | Add-Member -NotePropertyName matrix -NotePropertyValue @(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1)
}
$overflow.nodes[3].matrix[12] = 3.4028234663852886e38
$overflow.nodes[3].matrix[13] = 3.4028234663852886e38
$overflow | ConvertTo-Json -Depth 100 | Set-Content $overflowInput -Encoding utf8
Invoke-Cmd @('--input', $overflowInput, '--output', $output, '--rotate', '0', '0', '-45') 1 'transform'
if (Test-Path $output) { throw 'Matrix overflow wrote output.' }

# A copied executable needs neither the engine DLL nor viewer assets.
$standalone = Join-Path $root 'standalone'
New-Item -ItemType Directory -Path $standalone | Out-Null
Copy-Item -LiteralPath $Executable -Destination $standalone
$Executable = Join-Path $standalone ([IO.Path]::GetFileName($Executable))
Push-Location $standalone
try { Invoke-Cmd @('--input', $source, '--output', (Join-Path $root 'standalone.glb'), '--rotate', '0', '0', '90') }
finally { Pop-Location }
& $Verifier verify $source (Join-Path $root 'standalone.glb') rotate
if ($LASTEXITCODE -ne 0) { throw 'Standalone output verification failed.' }
Write-Host "Cmd round trips and failure contracts passed: $root"
