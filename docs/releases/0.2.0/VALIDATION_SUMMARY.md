# Elf3D 0.2.0 Validation Summary

Purpose: Record validation performed before publishing Elf3D 0.2.0.

Applicable version: 0.2.0

Document status: Release validation summary.

Last verified implementation commit: pending 0.2.0 release source commit

Implementation source paths: `CMakeLists.txt`, `CMakePresets.json`,
`.github/workflows/ci.yml`, `.github/workflows/release.yml`,
`scripts/package_release.ps1`, `tests`, `modules/*/tests`,
`tests/fixtures/textured_pbr.gltf`, `docs/releases/0.2.0`

Known limitations: External model corpus validation and performance
measurements have not run. Additional Computer Use screenshot capture in the
release clone was blocked by `GetCursorPos failed: Access denied (0x80070005)`;
the same viewer UI implementation had already been visually compared against
`Z:\Low\x64\low3D.exe` during the ordinary-change validation before release
versioning.

Related documents: `PROJECT_STATE_EN.md`, `KNOWN_LIMITATIONS.md`,
`RELEASE_CHECKLIST.md`, `RELEASE_ARTIFACTS.md`

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

`cmake` and `ctest` were not assumed to be on `PATH`; the Visual Studio bundled
executables above were used. FetchContent clone checkouts on `Z:` required
explicit Git `safe.directory` entries for the cloned `_deps/*-src` directories.

## Commands Executed

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --fresh --preset windows-debug
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build --preset windows-debug --parallel
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --preset windows-debug --output-on-failure
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --fresh --preset windows-release
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build --preset windows-release --parallel
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --preset windows-release --output-on-failure
.\scripts\package_release.ps1 -Version 0.2.0 -BuildDir out/build/windows-release -OutputDir out/release
```

Viewer smoke and package smoke were run with process window-handle checks,
`CloseMainWindow()`, and exit-code checks.

## Results

| Area | Result | Notes |
| --- | --- | --- |
| Debug configure | Passed | Fresh `windows-debug` preset configured after explicit FetchContent safe-directory setup. |
| Debug build | Passed | Built `elf3d.dll`, internal libraries, tests, `elf3d_imgui`, and `elf3d_viewer.exe`. |
| Debug tests | Passed | 16 passed, 0 failed, 0 skipped. |
| Release configure | Passed | Fresh `windows-release` preset configured. |
| Release build | Passed | Built Release targets in the separate Release tree. |
| Release tests | Passed | 16 passed, 0 failed, 0 skipped. |
| Release viewer smoke | Passed | Release viewer opened with `tests/fixtures/textured_pbr.gltf` and exited with code 0 after `CloseMainWindow()`. |
| Visual style comparison | Passed with limitation | The Low.3D-style UI implementation was compared visually during ordinary-change validation using screenshots of `Z:\Low\x64\low3D.exe` and the viewer. Additional release-clone Computer Use capture was blocked by Windows access denial. |
| Release package | Passed | `elf3d-viewer-0.2.0-windows-x64.zip` and `SHA256SUMS.txt` were generated. |
| Archive inspection | Passed | ZIP contained `assets/font/DroidSans.ttf`, all generated toolbar icons, runtime binaries, README, license files, and third-party notices. |
| Checksums | Passed | `SHA256SUMS.txt` matched the viewer ZIP hash. |
| Packaged viewer smoke | Passed | Extracted package viewer opened and exited with code 0 after `CloseMainWindow()`. |
| GitHub branch CI | Passed | Develop CI run `28129043437` and main CI run `28129065207` passed Debug and Release jobs. |
| Tag-triggered release workflow | Passed | Release workflow run `28159209602` built, tested, packaged, uploaded assets, and created the GitHub Release. |
| Published release assets | Passed | Downloaded GitHub Release assets; `SHA256SUMS.txt` matched the downloaded viewer ZIP hash `ad2fceae8ff0af7d83521a92e3e1cd20407ac209900ba62b3d8e544fe7ad9021`. |
| Public clone test | Passed | Fresh clone from `v0.2.0` configured, built, and passed Debug and Release CTest 16/16. |

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

- `out/release/elf3d-viewer-0.2.0-windows-x64.zip`
- `out/release/SHA256SUMS.txt`

SHA-256:

```text
8cdd519ccee832fb8705a307eba32e46759e8920fef37ea2993a0b85b599c3e4  elf3d-viewer-0.2.0-windows-x64.zip
```

ZIP size: 1,132,433 bytes.

Published GitHub Release asset SHA-256:

```text
ad2fceae8ff0af7d83521a92e3e1cd20407ac209900ba62b3d8e544fe7ad9021  elf3d-viewer-0.2.0-windows-x64.zip
```

Published ZIP size: 1,132,689 bytes.

## Not Verified

- External model corpus.
- Benchmark or performance measurements.

## Release Validation Decision

`GO - ready for public release`

Local build, tests, package creation, archive inspection, release viewer smoke,
packaged viewer smoke, checksum verification, prior visual comparison of the
unchanged viewer UI implementation, GitHub CI, tag-triggered release workflow,
GitHub Release asset verification, and public clone testing passed. Continue
only with final branch synchronization and remote verification.
