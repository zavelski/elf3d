# Shared discovery for Windows automation. Explicit overrides never silently fall back.
function Get-Elf3DVisualStudio {
    param([string]$VersionRange = '[18.0,)')
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio Installer/vswhere is required.' }
    $installation = & $vswhere -latest -products '*' -version $VersionRange `
        -requires Microsoft.Component.MSBuild Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($installation)) {
        throw 'Visual Studio 2026 or newer with MSBuild and Desktop C++ tools is required.'
    }
    return $installation
}

function Get-Elf3DCMakeVersion {
    param([string]$Path, [string]$Name)
    $output = @(& $Path --version 2>&1) -join "`n"
    if ($LASTEXITCODE -ne 0 -or $output -notmatch "(?m)^$Name version ([0-9]+\.[0-9]+\.[0-9]+)(\r?$|\s)") {
        throw "Could not read a stable $Name version from '$Path'."
    }
    $version = [version]$Matches[1]
    if ($version -lt [version]'4.4.3') { throw "$Name 4.4.3 or newer is required; found $version at '$Path'." }
    return $version
}

function Resolve-Elf3DCMakeTools {
    param([string]$CMakePath = '', [string]$CTestPath = '')
    $candidates = @()
    $failures = @()
    if ($CMakePath) {
        $candidates = @((Resolve-Path -LiteralPath $CMakePath).Path)
    } else {
        $candidates = @(Get-Command cmake -CommandType Application -All -ErrorAction SilentlyContinue |
            Select-Object -ExpandProperty Source)
        $standalone = Join-Path $env:ProgramFiles 'CMake/bin/cmake.exe'
        if (Test-Path -LiteralPath $standalone) { $candidates += $standalone }
        try {
            $installation = Get-Elf3DVisualStudio
            $bundled = Join-Path $installation 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
            if (Test-Path -LiteralPath $bundled) { $candidates += $bundled }
        } catch { $failures += $_.Exception.Message }
    }
    foreach ($candidate in $candidates | Select-Object -Unique) {
        try {
            $cmakeVersion = Get-Elf3DCMakeVersion $candidate 'cmake'
            $directory = Split-Path -Parent $candidate
            $ctest = if ($CTestPath) { (Resolve-Path -LiteralPath $CTestPath).Path } else { Join-Path $directory 'ctest.exe' }
            $ctestVersion = Get-Elf3DCMakeVersion $ctest 'ctest'
            $cpack = Join-Path $directory 'cpack.exe'
            $cpackVersion = Get-Elf3DCMakeVersion $cpack 'cpack'
            if ($cmakeVersion -ne $ctestVersion -or $cmakeVersion -ne $cpackVersion) {
                throw 'CMake, CTest and CPack must have identical versions.'
            }
            if ((Split-Path -Parent $ctest) -ne $directory) { throw 'CMake and CTest must come from the same installation.' }
            return [pscustomobject]@{CMake=$candidate; CTest=$ctest; CPack=$cpack; Version=$cmakeVersion}
        } catch {
            if ($CMakePath -or $CTestPath) { throw }
            $failures += $_.Exception.Message
        }
    }
    throw "No supported standalone or VS CMake suite was found. $($failures -join ' ')"
}
