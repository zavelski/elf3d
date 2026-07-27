[CmdletBinding()]
param(
    [string]$RepositoryRoot = "",
    [string]$ClangFormatExecutable = "clang-format",
    [string]$ExpectedVersion = "22.1.8"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
if (Get-Variable -Name PSNativeCommandUseErrorActionPreference -ErrorAction SilentlyContinue) {
    $PSNativeCommandUseErrorActionPreference = $false
}

if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
    $RepositoryRoot = Split-Path -Parent $PSScriptRoot
}
$repositoryPath = [System.IO.Path]::GetFullPath($RepositoryRoot)
if (-not (Test-Path -LiteralPath (Join-Path $repositoryPath ".clang-format") -PathType Leaf)) {
    throw "Repository .clang-format was not found below '$repositoryPath'."
}

$formatCommand = Get-Command $ClangFormatExecutable -ErrorAction SilentlyContinue
if ($null -eq $formatCommand) {
    throw "clang-format executable '$ClangFormatExecutable' was not found."
}

$versionOutput = @(& $formatCommand.Source --version 2>&1)
if ($LASTEXITCODE -ne 0) {
    throw "clang-format version query failed with exit code $LASTEXITCODE."
}
$versionMatch = [regex]::Match(
    ($versionOutput -join "`n"),
    "clang-format version ([0-9]+\.[0-9]+\.[0-9]+)"
)
if (-not $versionMatch.Success) {
    throw "Could not parse the clang-format version from '$($versionOutput -join " ")'."
}
$actualVersion = $versionMatch.Groups[1].Value
if ($actualVersion -ne $ExpectedVersion) {
    throw "Expected clang-format $ExpectedVersion, but found $actualVersion."
}

if ($null -eq (Get-Command git -ErrorAction SilentlyContinue)) {
    throw "Git is required to enumerate project-owned C++ files."
}

$patterns = @(
    "*.c",
    "*.cc",
    "*.cpp",
    "*.cxx",
    "*.h",
    "*.hh",
    "*.hpp",
    "*.hxx",
    "*.cppm",
    "*.ixx"
)
$trackedAndUntracked = @(
    & git -C $repositoryPath ls-files --cached --others --exclude-standard -- @patterns
)
if ($LASTEXITCODE -ne 0) {
    throw "Git failed to enumerate project-owned C++ files with exit code $LASTEXITCODE."
}

$relativeFiles = @(
    $trackedAndUntracked |
        Where-Object {
            $_ -notmatch "^third_party/" -and
            $_ -notmatch "^tools/(green-profile|module-graph)/fixtures/"
        } |
        Sort-Object -Unique
)
if ($relativeFiles.Count -eq 0) {
    throw "No project-owned C++ files were found."
}

$violations = [System.Collections.Generic.List[string]]::new()
foreach ($relativeFile in $relativeFiles) {
    $fullPath = Join-Path $repositoryPath $relativeFile
    if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
        throw "Enumerated C++ file '$relativeFile' does not exist."
    }

    $formatOutput = @(
        & $formatCommand.Source --dry-run --Werror --style=file $fullPath 2>&1
    )
    if ($LASTEXITCODE -eq 0) {
        continue
    }

    $violations.Add($relativeFile)
    Write-Host "Formatting violation: $relativeFile"
    foreach ($line in $formatOutput) {
        Write-Host "  $line"
    }
}

if ($violations.Count -ne 0) {
    throw "$($violations.Count) project-owned C++ file(s) do not match clang-format $ExpectedVersion."
}

Write-Host (
    "Formatting contract passed for $($relativeFiles.Count) project-owned C++ files " +
    "with clang-format $actualVersion."
)
