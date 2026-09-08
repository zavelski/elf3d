[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$BuildDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot 'solution-projects.ps1')

$buildPath = (Resolve-Path -LiteralPath $BuildDirectory).Path
$solutionPath = Join-Path $buildPath "Elf3D.slnx"
if (-not (Test-Path -LiteralPath $solutionPath -PathType Leaf)) {
    throw "Elf3D.slnx was not found in '$buildPath'. Configure a full Visual Studio preset first."
}

# Keep solution-relative paths for the solution filter.
$projects = @{}
$viewer = $null
foreach ($entry in Read-SolutionProjects $solutionPath) {
    $projects[$entry.Path] = $entry.RelativePath
    if ($entry.Name -eq 'elf3d_viewer') {
        $viewer = $entry.Path
    }
}
if ($null -eq $viewer) {
    throw "The solution has no elf3d_viewer project. Configure with ELF3D_BUILD_VIEWER=ON."
}

$pending = [System.Collections.Generic.Stack[string]]::new()
$visited = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase
)
$pending.Push($viewer)
while ($pending.Count -gt 0) {
    $projectPath = $pending.Pop()
    if (-not $visited.Add($projectPath)) {
        continue
    }
    if (-not $projects.ContainsKey($projectPath)) {
        throw "Referenced project '$projectPath' does not belong to '$solutionPath'."
    }
    if (-not (Test-Path -LiteralPath $projectPath -PathType Leaf)) {
        throw "Referenced project '$projectPath' does not exist. Regenerate the solution."
    }
    [xml]$project = Get-Content -LiteralPath $projectPath -Raw
    # ItemDefinitionGroup also contains ProjectReference defaults without Include.
    foreach ($reference in $project.SelectNodes("//*[local-name()='ItemGroup']/*[local-name()='ProjectReference']")) {
        $referencePath = $reference.GetAttribute("Include")
        if ([string]::IsNullOrWhiteSpace($referencePath)) {
            throw "ProjectReference in '$projectPath' is missing a valid Include path."
        }
        $absoluteReference = [System.IO.Path]::GetFullPath(
            [System.IO.Path]::Combine((Split-Path -Parent $projectPath), $referencePath)
        )
        $pending.Push($absoluteReference)
    }
}

$filter = [ordered]@{
    solution = [ordered]@{
        path = "Elf3D.slnx"
        projects = @($visited | ForEach-Object { $projects[$_] } | Sort-Object)
    }
}
$filterPath = Join-Path $buildPath "Elf3D-Study.slnf"
$filter | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $filterPath -Encoding utf8
Write-Host "Created '$filterPath' with $($visited.Count) projects. The full solution is unchanged."
