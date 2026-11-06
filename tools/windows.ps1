Set-StrictMode -Version Latest

function Get-Elf3DToolchain {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (!(Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio Installer/vswhere is required.' }
    $installation = & $vswhere -latest -products '*' -version '[18.0,19.0)' `
        -requires Microsoft.Component.MSBuild Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (!$installation -or $LASTEXITCODE) { throw 'Stable Visual Studio 2026 with Desktop C++ tools is required.' }
    $version = (Get-Content -LiteralPath (Join-Path $installation 'VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt') -Raw).Trim()
    $msbuild = Join-Path $installation 'MSBuild/Current/Bin/amd64/MSBuild.exe'
    if (!(Test-Path -LiteralPath $msbuild)) { throw "MSBuild is missing: $msbuild" }
    [pscustomobject]@{ Installation=$installation; MSBuild=$msbuild; VCToolsVersion=$version;
        Tools=(Join-Path $installation "VC/Tools/MSVC/$version/bin/Hostx64/x64");
        DeveloperCommand=(Join-Path $installation 'Common7/Tools/VsDevCmd.bat') }
}

function Get-Elf3DProjects {
    param([string]$RepositoryRoot)
    foreach ($file in Get-ChildItem -LiteralPath (Join-Path $RepositoryRoot 'win/projects') -Filter '*.vcxproj') {
        [xml]$xml = Get-Content -LiteralPath $file.FullName -Raw
        $properties = $xml.SelectSingleNode("//*[local-name()='PropertyGroup' and @Label='Globals']")
        [pscustomobject]@{ Name=$file.BaseName; Path=$file.FullName; Part=[string]$properties.Elf3DPart;
            Model=([string]$properties.Elf3DModelTarget -eq 'true'); Kind=[string]$properties.Elf3DOutputKind }
    }
}

function Invoke-Elf3DProcess {
    param([string]$File, [string[]]$Arguments, [string]$WorkingDirectory, [int]$Timeout=120)
    $start = [Diagnostics.ProcessStartInfo]::new($File)
    $start.WorkingDirectory=$WorkingDirectory
    $start.UseShellExecute=$false
    $start.RedirectStandardOutput=$true
    $start.RedirectStandardError=$true
    foreach ($argument in $Arguments) { $start.ArgumentList.Add($argument) }
    $process=[Diagnostics.Process]::new()
    $process.StartInfo=$start
    if (!$process.Start()) { throw "Cannot start $File" }
    $stdout=$process.StandardOutput.ReadToEndAsync()
    $stderr=$process.StandardError.ReadToEndAsync()
    if (!$process.WaitForExit($Timeout*1000)) {
        $process.Kill($true)
        $process.WaitForExit()
        throw "Timeout after $Timeout seconds: $File"
    }
    [pscustomobject]@{ ExitCode=$process.ExitCode; Output=$stdout.GetAwaiter().GetResult(); Error=$stderr.GetAwaiter().GetResult() }
    $process.Dispose()
}

function Import-Elf3DCompilerEnvironment {
    param($Toolchain)
    $start=[Diagnostics.ProcessStartInfo]::new($env:ComSpec)
    $start.Arguments='/d /s /c ""'+$Toolchain.DeveloperCommand+'" -arch=x64 -host_arch=x64 >nul && set PATH && set INCLUDE && set LIB"'
    $start.UseShellExecute=$false
    $start.CreateNoWindow=$true
    $start.RedirectStandardOutput=$true
    $process=[Diagnostics.Process]::Start($start)
    $variables=$process.StandardOutput.ReadToEnd().Split("`n")
    $process.WaitForExit()
    if ($process.ExitCode) { throw 'Could not initialize the VS 2026 compiler environment.' }
    $process.Dispose()
    foreach ($line in $variables) {
        if ($line.TrimEnd("`r") -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process') }
    }
    # VsDevCmd adds bundled CMake; native compilation must also work without it.
    $env:PATH=($env:PATH.Split(';') | Where-Object {
        $_ -and $_ -notmatch '(?i)cmake' -and
        !(Test-Path -LiteralPath (Join-Path $_ 'cmake.exe')) -and
        !(Test-Path -LiteralPath (Join-Path $_ 'ctest.exe')) -and
        !(Test-Path -LiteralPath (Join-Path $_ 'cpack.exe'))
    }) -join ';'
}
