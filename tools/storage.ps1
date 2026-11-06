Set-StrictMode -Version Latest

# Inspect before deleting: never traverse junctions or treat a repository as cache.
function Resolve-Elf3DGeneratedPath {
    param([Parameter(Mandatory)][string]$AllowedRoot,
        [Parameter(Mandatory)][string]$LiteralPath)
    $allowed=[IO.Path]::GetFullPath($AllowedRoot).TrimEnd('\','/')+'\'
    $resolved=[IO.Path]::GetFullPath($LiteralPath).TrimEnd('\','/')
    if (!$resolved.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)) {
        throw "Cleanup escapes its allowed root: $resolved"
    }
    # Checking only the leaf misses an out/ or validation/ junction.
    $ancestor=$resolved
    while ($ancestor) {
        if (Test-Path -LiteralPath $ancestor) {
            $item=Get-Item -LiteralPath $ancestor -Force
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "Cleanup refuses reparse point: $ancestor"
            }
        }
        $ancestor=Split-Path -Parent $ancestor
    }
    return $resolved
}

function Get-Elf3DGeneratedData {
    param([Parameter(Mandatory)][string]$AllowedRoot,
        [Parameter(Mandatory)][string]$LiteralPath)
    $resolved=Resolve-Elf3DGeneratedPath -AllowedRoot $AllowedRoot -LiteralPath $LiteralPath
    if (!(Test-Path -LiteralPath $resolved)) { return }
    $pending=[Collections.Generic.Stack[object]]::new()
    $pending.Push((Get-Item -LiteralPath $resolved -Force))
    $bytes=0L; $files=0; $newest=[datetime]::MinValue
    while ($pending.Count) {
        $item=$pending.Pop()
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
            throw "Cleanup refuses reparse point: $($item.FullName)"
        }
        if ($item.Name -eq '.git' -or ($item.PSIsContainer -and
                (Test-Path -LiteralPath (Join-Path $item.FullName 'HEAD') -PathType Leaf) -and
                (Test-Path -LiteralPath (Join-Path $item.FullName 'objects') -PathType Container))) {
            throw "Cleanup refuses a Git repository: $($item.FullName)"
        }
        if ($item.LastWriteTimeUtc -gt $newest) { $newest=$item.LastWriteTimeUtc }
        if ($item.PSIsContainer) {
            foreach ($child in Get-ChildItem -LiteralPath $item.FullName -Force) { $pending.Push($child) }
        } else { $bytes+=$item.Length; ++$files }
    }
    [pscustomobject]@{Path=$resolved;Bytes=$bytes;Files=$files;NewestWriteUtc=$newest}
}

function Remove-Elf3DGeneratedData {
    [CmdletBinding(SupportsShouldProcess)]
    param([Parameter(Mandatory)][string]$AllowedRoot,
        [Parameter(Mandatory)][string]$LiteralPath)
    $data=Get-Elf3DGeneratedData -AllowedRoot $AllowedRoot -LiteralPath $LiteralPath
    if ($data -and $PSCmdlet.ShouldProcess($data.Path,'Remove generated data')) {
        Remove-Item -LiteralPath $data.Path -Recurse -Force -ErrorAction Stop
        return $data
    }
}
