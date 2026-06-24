# Elf3D 0.1.0 Validation Summary

Purpose: Record local validation performed before any public publication of
Elf3D 0.1.0.

Applicable version: 0.1.0

Document status: Publication validation summary.

Last verified implementation commit before final package-record update:
`a99bb1008882994d3127141019b049927cbc2c97`

Implementation source paths: `CMakeLists.txt`, `CMakePresets.json`,
`.github/workflows/ci.yml`, `.github/workflows/release.yml`,
`scripts/package_release.ps1`, `tests`, `modules/*/tests`,
`tests/fixtures/textured_pbr.gltf`, `docs/releases/0.1.0`

Known limitations: Manual interaction validation was user-performed on the
packaged Windows Release viewer and is not automated. Tag-triggered GitHub
Release validation, public clone testing, external model corpus validation,
and performance measurements have not yet run.

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

Final local validation was rerun on 2026-06-24 after the manual interaction
validation `GO` decision. Release validation was rerun again after the
GitHub Actions workflow correction was merged to `main`:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --fresh --preset windows-debug
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build --preset windows-debug
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --preset windows-debug --output-on-failure
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --fresh --preset windows-release
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build --preset windows-release
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --preset windows-release --output-on-failure
.\scripts\package_release.ps1 -Version 0.1.0
```

Both final fresh configure runs completed without temporary Git safe-directory
overrides.

The final package was regenerated after the GitHub Actions workflow correction.
The final assets listed below are from the successful package run.

Viewer smoke and package smoke were run with PowerShell `Start-Process`,
window-handle checks, `CloseMainWindow()`, and exit-code checks.

## Results

| Area | Result | Notes |
| --- | --- | --- |
| Debug configure | Passed | Fresh `windows-debug` preset configure completed after the `GO` decision. |
| Debug build | Passed | Built `elf3d.dll`, internal libraries, tests, `elf3d_imgui`, and `elf3d_viewer.exe`. |
| Debug warnings | None observed | Build output did not contain warning diagnostics. |
| Debug tests | Passed | 16 passed, 0 failed, 0 skipped. |
| Release configure | Passed | Fresh `windows-release` preset configure completed after the `GO` decision. |
| Release build | Passed | Built Release targets in the separate Release tree. |
| Release warnings | None observed | Build output did not contain warning diagnostics. |
| Release tests | Passed | 16 passed, 0 failed, 0 skipped. |
| Viewer smoke | Passed | Debug and Release viewers opened with `tests/fixtures/textured_pbr.gltf` and exited with code 0 after `CloseMainWindow()`. |
| Screenshot visual check | Passed with limitation | Release screenshot showed the fixture rendered in the viewer. Interaction was not fully exercised. |
| Release package | Passed | `elf3d-viewer-0.1.0-windows-x64.zip` and `SHA256SUMS.txt` regenerated after the final Release build on corrected `main`. |
| Archive inspection | Passed | ZIP contents matched the planned viewer package file list. |
| Packaged viewer smoke | Passed | Extracted final package opened from the extracted package directory and exited with code 0 after `CloseMainWindow()`. |
| GitHub branch CI | Passed | Corrected `develop` and `main` CI ran on `windows-2022`, used `actions/checkout@v6`, configured with MSVC/Visual Studio 2022, built, and passed 16/16 tests in Debug and Release. |
| Manual viewer interaction matrix | Passed | User manually validated the packaged Windows Release viewer. This was not an automated test. |

## Manual Interaction Matrix

The following packaged Windows Release viewer scenarios were manually tested
by the user and passed:

- orbit navigation
- pan navigation
- wheel zoom
- Fit to Scene
- Reset View
- Viewport picking
- object selection and highlighting
- synchronization between Viewport selection and Scene Hierarchy
- Hide Selected
- Show Selected
- Show All
- inherited hierarchy visibility
- Isolate Selected
- Exit Isolation
- point-to-point distance measurement
- measurement stability during camera navigation
- section-plane clipping
- retained-side switching
- clipping boxes
- Clear Clipping
- rejection of clipped geometry by picking
- Reload
- Close Scene
- loading a Scene after Close Scene
- failed-load preservation of the previously active Scene
- normal viewer shutdown

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
1d39c50460e86083f448557ed6a7eddad3974d26b99e84e4c2cfc030c5265c92  elf3d-viewer-0.1.0-windows-x64.zip
```

## Not Verified

- Tag-triggered release workflow on GitHub.
- Public clone test.
- External model corpus.
- Benchmark or performance measurements.

## Release Validation Decision

`GO — ready for public publication`

Local build, tests, package creation, archive inspection, limited viewer
smoke, packaged viewer smoke, and user-performed packaged viewer interaction
validation passed. Continue with final local validation and publication steps;
restore no-go if any required validation fails.
