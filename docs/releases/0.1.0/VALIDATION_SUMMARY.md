# Elf3D 0.1.0 Validation Summary

Purpose: Record final release-candidate validation performed for Elf3D 0.1.0.

Applicable version: 0.1.0

Document status: Release snapshot summary.

Last verified implementation commit before release snapshot: `79fd4bc`

Implementation source paths: `CMakePresets.json`, `tests`,
`modules/*/tests`, `tests/fixtures/textured_pbr.gltf`,
`docs/audits/ELF3D_0.1.0_VALIDATION_MATRIX.md`

Known limitations: Viewer validation was process smoke only. Manual rendering,
interaction, and clean shutdown were not verified.

Related documents: `PROJECT_STATE_EN.md`, `AUDIT_SUMMARY.md`,
`KNOWN_LIMITATIONS.md`, `RELEASE_CHECKLIST.md`

## Environment

- Host shell: PowerShell
- CMake/CTest:
  `C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin`
- CMake/CTest version: `3.31.6-msvc6`
- Generator: `Visual Studio 17 2022`
- Architecture: `x64`
- Visual Studio: `17.14.23`
- MSVC: `19.44.35222.0`
- Windows SDK selected by CMake: `10.0.26100.0`
- Target Windows reported by CMake: `10.0.26200`

## Commands Executed

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --fresh --preset windows-debug
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build --preset windows-debug
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --preset windows-debug --output-on-failure
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --fresh --preset windows-release
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build --preset windows-release
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --preset windows-release --output-on-failure
```

Viewer smoke command pattern:

```powershell
$fixture = Resolve-Path 'tests\fixtures\textured_pbr.gltf'
foreach ($config in @('Debug','Release')) {
    $viewer = Resolve-Path "out\build\windows-debug\bin\$config\elf3d_viewer.exe"
    $process = Start-Process -FilePath $viewer -ArgumentList @($fixture) -PassThru -WindowStyle Hidden
    Start-Sleep -Seconds 5
    Stop-Process -Id $process.Id
}
```

Header self-containment command pattern:

```powershell
cmd /d /s /c "`"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat`" -arch=x64 >NUL && cl /nologo /std:c++20 /permissive- /EHsc /W4 /WX /I`"Z:\Elf3D\include`" /FI`"Z:\Elf3D\include\elf3d\elf3d.h`" /c `"Z:\Elf3D\out\header-check\header_check_empty.cpp`""
```

## Results

| Area | Result | Notes |
| --- | --- | --- |
| Debug configure | Passed | Fresh preset configure completed. |
| Debug build | Passed | Built `elf3d.dll`, internal libraries, tests, `elf3d_imgui`, and `elf3d_viewer.exe`. |
| Debug warnings | None observed | Build output did not contain warning diagnostics. |
| Debug tests | Passed | 16 passed, 0 failed, 0 skipped. |
| Release configure | Passed | Fresh preset configure completed. |
| Release build | Passed | Built Release targets in the multi-config tree. |
| Release warnings | None observed | Build output did not contain warning diagnostics. |
| Release tests | Passed | 16 passed, 0 failed, 0 skipped. |
| Public headers | Passed | All public headers compiled individually as forced includes with MSVC C++20. |
| Debug viewer smoke | Passed | Process stayed alive for five seconds with `tests/fixtures/textured_pbr.gltf`, then was intentionally terminated. |
| Release viewer smoke | Passed | Process stayed alive for five seconds with `tests/fixtures/textured_pbr.gltf`, then was intentionally terminated. |
| Documentation path/link check | Passed | Required path check covered 43 paths; Markdown link check covered 31 Markdown files. |

## CTest Totals

Debug:

- 16 passed
- 0 failed
- 0 skipped

Release:

- 16 passed
- 0 failed
- 0 skipped

## Not Verified

- Human visual rendering correctness.
- Viewer navigation, picking, selection, visibility, isolation, measurement,
  and clipping interaction.
- Normal user-driven viewer shutdown.
- External model corpus.
- Benchmark or performance measurements.
- CI.

## Release Validation Decision

`Not ready due to release blockers`

The automated and smoke validation passed, but manual visual viewer validation
is still required before integration and tagging.
