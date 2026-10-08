# Read only the CMake-generated XML solution format supported by Elf3D.
function Read-SolutionProjects {
    param([Parameter(Mandatory)][string]$SolutionPath)

    if ([System.IO.Path]::GetExtension($SolutionPath) -ne '.slnx') {
        throw 'Elf3D supports only .slnx solutions. Regenerate with Visual Studio 2026 or newer.'
    }
    if (-not (Test-Path -LiteralPath $SolutionPath -PathType Leaf)) {
        throw "Solution '$SolutionPath' does not exist. Regenerate the solution."
    }
    $solutionFile = (Resolve-Path -LiteralPath $SolutionPath -ErrorAction Stop).Path
    $directory = Split-Path -Parent $solutionFile
    [xml]$solution = Get-Content -LiteralPath $solutionFile -Raw
    if ($solution.DocumentElement.Name -ne 'Solution') {
        throw "Invalid XML solution: '$solutionFile'."
    }
    $seen = [System.Collections.Generic.HashSet[string]]::new(
        [System.StringComparer]::OrdinalIgnoreCase)
    foreach ($project in $solution.SelectNodes('/Solution//Project')) {
        $relativePath = $project.GetAttribute('Path')
        if ([string]::IsNullOrWhiteSpace($relativePath)) { throw 'A solution Project is missing its Path attribute.' }
        if ([System.IO.Path]::GetExtension($relativePath) -ne '.vcxproj') { continue }
        $path = [System.IO.Path]::GetFullPath(
            [System.IO.Path]::Combine($directory, $relativePath))
        if (-not $seen.Add($path)) { throw "Duplicate solution project: '$relativePath'." }
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Solution project '$relativePath' does not exist. Regenerate the solution."
        }
        [pscustomobject]@{
            Name = [System.IO.Path]::GetFileNameWithoutExtension($relativePath)
            RelativePath = $relativePath
            Path = $path
            DefaultStartup = $project.GetAttribute('DefaultStartup') -eq 'true'
        }
    }
}

function Get-SolutionStartupProject {
    param([Parameter(Mandatory)][object[]]$Projects)
    $startup = @($Projects | Where-Object DefaultStartup)
    if ($startup.Count -ne 1) {
        throw 'The generated solution must specify exactly one default startup project.'
    }
    return $startup[0]
}
