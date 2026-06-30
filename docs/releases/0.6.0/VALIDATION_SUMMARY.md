# Elf3D 0.6.0 Validation Summary

Purpose: Record local validation performed for the Elf3D 0.6.0 release source.

Applicable version: 0.6.0

Document status: Local release validation summary.

Release source identifier: local tag `v0.6.0` after release commit.

Implementation source paths: `CMakeLists.txt`, `CMakePresets.json`,
`.github/workflows/ci.yml`, `.github/workflows/release.yml`,
`scripts/package_release.ps1`, `tests`, `modules/*/tests`,
`tests/fixtures/textured_pbr.gltf`, `third_party`, `docs/releases/0.6.0`

Known limitations: Complete manual viewer interaction, visual rendering,
external model corpus, performance measurements, GitHub CI, tag workflow,
published-asset verification, and public clone testing have not run.

Related documents: `PROJECT_STATE_EN.md`, `KNOWN_LIMITATIONS.md`,
`RELEASE_CHECKLIST.md`, `RELEASE_ARTIFACTS.md`

## Environment

- Host shell: PowerShell
- CMake/CTest: Visual Studio bundled `3.31.6-msvc6`
- Generator: `Visual Studio 17 2022`
- Architecture: `x64`
- MSBuild: `17.14.40.60911`
- MSVC: `19.44.35228.0`
- Windows SDK: `10.0.26100.0`
- Target Windows reported by CMake: `10.0.26200`
- MSVC runtime: dynamic (`/MDd` Debug, `/MD` Release)

## Commands Executed

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --fresh --preset windows-debug
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build --preset windows-debug --parallel
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --preset windows-debug --output-on-failure
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --fresh --preset windows-release
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build --preset windows-release --parallel
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --preset windows-release --output-on-failure
.\scripts\package_release.ps1 -Version 0.6.0 -BuildDir out/build/windows-release -OutputDir out/release
```

## Results

| Area | Result | Notes |
| --- | --- | --- |
| Debug configure | Passed | Fresh `windows-debug` preset configured with CMake 3.31.6. |
| Debug build | Passed | Built `elf3d.dll`, all module targets, tests, ImGui integration, and viewer. |
| Debug tests | Passed | 17/17 passed. |
| Release configure | Passed | Fresh `windows-release` preset configured with CMake 3.31.6. |
| Release build | Passed | Built all Release targets, including `elf3d.dll` and `elf3d_viewer.exe`. |
| Release tests | Passed | 17/17 passed. |
| Vendored dependency configure | Passed | No `_deps/*-src` dependency source downloads were created under `out/build`. |
| Release package | Passed | Generated the 0.6.0 viewer ZIP and checksum file. |
| Archive inspection | Passed | 29 entries; runtime binaries, assets, licenses, and notices present; build artifacts absent. |
| Checksums | Passed | ZIP SHA-256 matched `SHA256SUMS.txt`. |
| Visual and interaction validation | Not run | Requires manual use of the packaged viewer. |
| GitHub validation | Not run | No push, CI run, GitHub Release, asset download, or public clone was performed. |

## Release Assets

```text
1aa9b3e8415bfb0c5368df27808d69c1d8272a1221f37a1df0a3375bfa247861  elf3d-viewer-0.6.0-windows-x64.zip
```

ZIP size: 1,502,327 bytes.

## Release Validation Decision

`LOCAL-GO - local source/package validated; publication deferred`
