# Elf3D 0.4.0 Validation Summary

Purpose: Record local validation performed for the Elf3D 0.4.0 release source.

Applicable version: 0.4.0

Document status: Local release validation summary.

Last verified implementation commit: pending 0.4.0 release source commit

Implementation source paths: `CMakeLists.txt`, `CMakePresets.json`,
`.github/workflows/ci.yml`, `.github/workflows/release.yml`,
`scripts/package_release.ps1`, `tests`, `modules/*/tests`,
`tests/fixtures/textured_pbr.gltf`, `docs/releases/0.4.0`

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
- MSBuild: `17.14.40`
- MSVC: `19.44.35228.0`
- Windows SDK: `10.0.26100.0`
- Target Windows reported by CMake: `10.0.26200`

## Commands Executed

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --fresh --preset windows-debug
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build --preset windows-debug -- /m:1
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --preset windows-debug --output-on-failure
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --fresh --preset windows-release
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build --preset windows-release -- /m:1
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --preset windows-release --output-on-failure
& .\scripts\package_release.ps1 -Version 0.4.0 -BuildDir out/build/windows-release -OutputDir out/release
```

## Results

| Area | Result | Notes |
| --- | --- | --- |
| Debug configure | Passed | Fresh `windows-debug` preset configured with CMake 3.31.6. |
| Debug build | Passed | Built `elf3d.dll`, all module targets, tests, ImGui integration, and viewer. |
| Debug tests | Passed after correction | Initial run exposed a stale `0.3.0` expectation in `elf3d.module_import_smoke`; after updating it to `0.4.0`, 17/17 passed. |
| Release configure | Passed | Fresh `windows-release` preset configured. |
| Release build | Passed | Built all Release targets without project warnings. |
| Release tests | Passed | 17/17 passed. |
| Module migration | Passed | Every internal named module imports through `elf3d.module_import_smoke`; no import-only shim header remains. |
| Release package | Passed | Generated the 0.4.0 viewer ZIP and checksum file. |
| Archive inspection | Passed | 28 entries; runtime binaries, assets, licenses, and notices present; build artifacts absent. |
| Checksums | Passed | ZIP SHA-256 matched `SHA256SUMS.txt`. |
| Packaged viewer smoke | Passed | Extracted viewer created a native window and exited with code 0 after `CloseMainWindow()`. |
| Workflow syntax | Passed | `.github/workflows/release.yml` parsed as YAML after 0.4.0 updates. |
| Visual and interaction validation | Not run | Requires manual use of the packaged viewer. About was not rechecked per user instruction. |
| GitHub validation | Not run | No push, tag, CI run, GitHub Release, asset download, or public clone was performed. |

## Release Assets

```text
160faae3e52d1f83514097a3a90bb42a011eab4f984b10418561f64ac7a4d266  elf3d-viewer-0.4.0-windows-x64.zip
```

ZIP size: 1,475,991 bytes.

## Release Validation Decision

`NO-GO - publication blocked`

Local automated gates, packaging, archive inspection, checksum verification,
and process smoke passed. Publication remains blocked until complete manual
viewer validation and all remote release gates pass.
