# Elf3D 0.7.1 Validation Summary

Purpose: Record local validation performed for the Elf3D 0.7.1 release source.

Applicable version: 0.7.1

Document status: Local release validation summary.

Last verified Git commit: local tag `v0.7.1` after release commit

Implementation source paths: `CMakeLists.txt`, `CMakePresets.json`,
`.github/workflows/ci.yml`, `.github/workflows/release.yml`,
`scripts/package_release.ps1`, `tests`, `modules/*/tests`,
`tests/fixtures/textured_pbr.gltf`, `third_party`, `docs/releases/0.7.1`

Known limitations: A private external model corpus, performance measurements,
GitHub CI, tag workflow, published-asset verification, public clone testing,
and comprehensive visual/manual viewer validation have not run.

Related documents: `PROJECT_STATE_EN.md`, `KNOWN_LIMITATIONS.md`,
`RELEASE_CHECKLIST.md`, `RELEASE_ARTIFACTS.md`

## Environment

- Host shell: PowerShell
- CMake/CTest: Visual Studio bundled `3.31.6-msvc6`
- Generator: `Visual Studio 17 2022`
- Architecture: `x64`
- MSBuild: `17.14.40+3e7442088`
- MSVC: `19.44.35228.0`
- Windows SDK: `10.0.26100.0`
- Target Windows reported by CMake: `10.0.26200`
- MSVC runtime: dynamic (`/MDd` Debug, `/MD` Release)

## Commands Executed

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --preset windows-debug
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build --preset windows-debug --parallel
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --preset windows-debug --output-on-failure
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --fresh --preset windows-release
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build --preset windows-release --parallel
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --preset windows-release --output-on-failure
.\out\build\windows-release\bin\Release\elf3d_gltf_probe.exe .\tests\fixtures
.\scripts\package_release.ps1 -Version 0.7.1 -BuildDir build/dist -OutputDir out/release
Expand-Archive -LiteralPath out/release/elf3d-viewer-0.7.1-windows-x64.zip -DestinationPath out/release/verify-0.7.1-final
```

## Results

| Area | Result | Notes |
| --- | --- | --- |
| Debug configure | Passed | `windows-debug` preset configured with CMake 3.31.6. |
| Debug build | Passed | Built `elf3d.dll`, all module targets, tests, ImGui integration, and viewer. |
| Debug tests | Passed | 18/18 passed, including real OpenGL shader/pixel smoke. |
| Release configure | Passed | Fresh `windows-release` preset configured with CMake 3.31.6. |
| Release build | Passed | Built all Release targets, including `elf3d.dll`, tests, ImGui integration, and `elf3d_viewer.exe`. |
| Release tests | Passed | 18/18 passed, including real OpenGL shader/pixel smoke. |
| Vendored dependency configure | Passed | No `_deps/*-src` dependency source downloads were created under `out/build`. |
| glTF fixture probe | Passed | `tests/fixtures/textured_pbr.gltf` loaded successfully: 1 file, 0 failures, 0 files with warnings. |
| External glTF corpus | Not run | No `out/local-gltf-corpus` directory was present in the workspace. |
| Release package | Passed | Generated the 0.7.1 viewer ZIP and checksum file. The local workspace materialized Release binaries under `build/dist/Release`, so packaging used `-BuildDir build/dist`. |
| Archive inspection | Passed | 29 entries; runtime binaries, assets, licenses, and notices present; build artifacts absent. |
| Checksums | Passed | ZIP SHA-256 matched `SHA256SUMS.txt`. |
| Viewer interaction validation | Not run | No visible/manual viewer interaction pass was performed after the 0.7.1 fixes. |
| Packaged viewer smoke | Passed locally | Extracted ZIP viewer process stayed alive for 5 seconds before being stopped. This is a process smoke, not visual validation. |
| GitHub validation | Not run | No push, CI run, GitHub Release, asset download, or public clone was performed. |

## Release Assets

```text
3e05fbd8bcb554ea95392762257cc62c72f52714d2450da8bc237c6ed4428477  elf3d-viewer-0.7.1-windows-x64.zip
```

ZIP size: 1,503,325 bytes.

## Release Validation Decision

`LOCAL-GO_WITH_LIMITATION - automated source/package validation passed; visible manual viewer, GitHub, and public clone validation remain deferred`
