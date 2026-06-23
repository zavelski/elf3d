# Elf3D 0.1.0 Validation Summary

Purpose: Record local validation performed before any public publication of
Elf3D 0.1.0.

Applicable version: 0.1.0

Document status: Publication validation summary.

Last verified implementation commit: `eeb39cdb2a9e92e61001a00d11cbe1880716f921`

Implementation source paths: `CMakeLists.txt`, `CMakePresets.json`,
`.github/workflows/ci.yml`, `.github/workflows/release.yml`,
`scripts/package_release.ps1`, `tests`, `modules/*/tests`,
`tests/fixtures/textured_pbr.gltf`, `docs/releases/0.1.0`

Known limitations: Viewer interaction validation is still incomplete. The
viewer was launched, rendered the project-owned fixture, captured to a
screenshot, and closed normally, but navigation, picking, selection,
visibility, isolation, measurement, and clipping were not manually exercised
end to end in the GUI.

Related documents: `PROJECT_STATE_EN.md`, `AUDIT_SUMMARY.md`,
`KNOWN_LIMITATIONS.md`, `RELEASE_CHECKLIST.md`, `RELEASE_ARTIFACTS.md`

## Environment

- Host shell: PowerShell
- CMake/CTest:
  `C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin`
- CMake/CTest version: `3.31.6-msvc6`
- Generator: `Visual Studio 17 2022`
- Architecture: `x64`
- Visual Studio/MSBuild: `17.14.23`
- MSVC: `19.44.35222.0`
- Windows SDK selected by CMake: `10.0.26100.0`
- Target Windows reported by CMake: `10.0.26200`

`cmake` and `ctest` were not on `PATH`; the Visual Studio bundled executables
above were used.

## Commands Executed

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --fresh --preset windows-debug
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build --preset windows-debug
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --preset windows-debug --output-on-failure
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --fresh --preset windows-release
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build --preset windows-release
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --preset windows-release --output-on-failure
.\scripts\package_release.ps1 -Version 0.1.0
```

The first Release configure attempt failed on a local Git safety check while
checking out a FetchContent dependency under `out/build/windows-release/_deps`
on the `Z:` filesystem. The successful retry used temporary per-process
`GIT_CONFIG_COUNT` environment entries for the generated dependency checkout
directories; no repository files or global Git config were changed. The retry
outlived the command timeout but completed and produced a valid
`out/build/windows-release/CMakeCache.txt`.

Viewer smoke and package smoke were run with PowerShell `Start-Process`,
window-handle checks, `CloseMainWindow()`, and exit-code checks.

## Results

| Area | Result | Notes |
| --- | --- | --- |
| Debug configure | Passed | Fresh preset configure completed. |
| Debug build | Passed | Built `elf3d.dll`, internal libraries, tests, `elf3d_imgui`, and `elf3d_viewer.exe`. |
| Debug warnings | None observed | Build output did not contain warning diagnostics. |
| Debug tests | Passed | 16 passed, 0 failed, 0 skipped. |
| Release configure | Passed with local workaround | Separate `out/build/windows-release` tree configured after temporary per-process Git safe-directory entries. |
| Release build | Passed | Built Release targets in the separate Release tree. |
| Release warnings | None observed | Build output did not contain warning diagnostics. |
| Release tests | Passed | 16 passed, 0 failed, 0 skipped. |
| Viewer smoke | Passed | Debug and Release viewers opened with `tests/fixtures/textured_pbr.gltf` and exited with code 0 after `CloseMainWindow()`. |
| Screenshot visual check | Passed with limitation | Release screenshot showed the fixture rendered in the viewer. Interaction was not fully exercised. |
| Release package | Passed | `elf3d-viewer-0.1.0-windows-x64.zip` and `SHA256SUMS.txt` created. |
| Archive inspection | Passed | ZIP contents matched the planned viewer package file list. |
| Packaged viewer smoke | Passed | Extracted package opened from outside the build tree and exited with code 0. |

## CTest Totals

Debug:

- 16 passed
- 0 failed
- 0 skipped

Release:

- 16 passed
- 0 failed
- 0 skipped

## Release Assets

- `out/release/elf3d-viewer-0.1.0-windows-x64.zip`
- `out/release/SHA256SUMS.txt`

SHA-256:

```text
7da7950c91fbabfa60ac35b7e2d4aa7f1387762ed587096aa5ebd86613ed72e5  elf3d-viewer-0.1.0-windows-x64.zip
```

## Not Verified

- Full manual viewer interaction matrix:
  navigation, picking, selection, hierarchy visibility, isolation,
  measurement, section plane controls, clipping boxes, reload, close scene, and
  failed-load preservation.
- Remote GitHub Actions CI.
- Tag-triggered release workflow on GitHub.
- Public clone test.
- External model corpus.
- Benchmark or performance measurements.

## Release Validation Decision

`NO-GO — publication blocked`

Local build, tests, package creation, archive inspection, and limited viewer
smoke passed. Public publication remains blocked because full manual viewer
interaction validation has not been completed and remote CI/release validation
cannot be run until after a later `GO` decision.
