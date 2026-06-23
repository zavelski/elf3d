# Testing

Purpose: Document how Elf3D 0.1.0 is configured, built, tested, and manually
validated.

Applicable version: 0.1.0

Document status: Verified from CMake presets, test targets, and Goal 3/4
validation on 2026-06-23.

Last verified Git commit: `8504068`

Implementation source paths: `CMakePresets.json`, `tests`,
`modules/*/tests`, `docs/audits/ELF3D_0.1.0_VALIDATION_MATRIX.md`

Known limitations: No CI workflow is present. Manual viewer interaction and
visual rendering were not validated during the recorded automated runs.

Related documents: `MODULE_MAP.md`, `USER_GUIDE.md`,
`PERFORMANCE_BASELINE.md`, `audits/ELF3D_0.1.0_VALIDATION_MATRIX.md`

## Environment

Validated environment:

- Visual Studio 2022 `17.14.23`
- MSVC `19.44.35222.0`
- Windows SDK `10.0.26100.0`
- CMake/CTest `3.31.6-msvc6` from Visual Studio bundled tools
- Generator: `Visual Studio 17 2022`
- Architecture: `x64`

In the validated shell, `cmake` and `ctest` were not on `PATH`. Use a Visual
Studio Developer PowerShell or call the bundled tools by absolute path.

## Configure, Build, Test

```powershell
cmake --fresh --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug --output-on-failure

cmake --fresh --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release --output-on-failure
```

If `cmake` is not on `PATH`, the validated executable was:

```powershell
C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe
```

The matching `ctest.exe` is in the same directory.

## Test Targets

| CTest name | Executable | Coverage |
| --- | --- | --- |
| `elf3d.math_conventions` | `elf3d_math_test` | value conversion and math conventions |
| `elf3d.interaction` | `elf3d_interaction_test` | viewport input transitions |
| `elf3d.assets` | `elf3d_assets_test` | mesh, image, texture, material validation |
| `elf3d.image_decode` | `elf3d_image_test` | bounded PNG/JPEG decode |
| `elf3d.scene` | `elf3d_scene_test` | entities, hierarchy, transforms, visibility, bounds |
| `elf3d.clipping` | `elf3d_clipping_test` | clipping filters and bounds math |
| `elf3d.navigation` | `elf3d_navigation_test` | orbit navigation, fit/reset |
| `elf3d.picking` | `elf3d_picking_test` | rays, AABB/triangle tests, BVH, filters |
| `elf3d.selection` | `elf3d_selection_test` | selection controller |
| `elf3d_tool_visibility_tests` | same | isolation and visibility filter state |
| `elf3d_tool_measurement_tests` | same | anchors, unit conversion, overlays |
| `elf3d_tool_clipping_tests` | same | clipping controller and helper overlays |
| `elf3d.gltf_import` | `elf3d_gltf_test` | glTF/GLB importer subset and failures |
| `elf3d.renderer` | `elf3d_renderer_test` | renderer preparation and caches |
| `elf3d.viewport_lifetime` | `elf3d_viewport_test` | viewport with fake graphics device |
| `elf3d.public_api_lifetime` | `elf3d_public_api_test` | public API smoke, version, load, lifetime |

Debug and Release both passed 16 of 16 tests after the Goal 4 lifetime fix.
Goal 7 repeated Debug and Release configure/build/CTest successfully.

## Public Header Self-Containment

Goal 7 compiled every public header under `include/elf3d` individually as a
forced include using MSVC C++20, `/permissive-`, `/W4`, and `/WX`. This checks
that each public header can be included first by a host translation unit.

## Fixtures

`tests/fixtures/textured_pbr.gltf` is project-owned and used for visual
validation. It contains asymmetric color corners, repeated and clamped
sampling, multiple materials, metallic and non-metallic surfaces, and no
external asset license requirement.

## Viewer Smoke Procedure

Non-interactive smoke used in Goal 3:

```powershell
$viewer = Resolve-Path 'out\build\windows-debug\bin\Debug\elf3d_viewer.exe'
$fixture = Resolve-Path 'tests\fixtures\textured_pbr.gltf'
$process = Start-Process -FilePath $viewer -ArgumentList @($fixture) -PassThru
Start-Sleep -Seconds 5
Stop-Process -Id $process.Id
```

This verifies that the process starts and remains alive briefly. It does not
verify rendering correctness, interaction, or clean user shutdown.

## Manual Validation Checklist

Before release, manually validate:

- viewer starts and shuts down cleanly
- procedural cube renders
- `tests/fixtures/textured_pbr.gltf` loads and renders
- failed load preserves current scene
- orbit, pan, wheel dolly, fit, reset
- picking and selection
- hierarchy selection and visibility
- isolation and exit isolation
- distance measurement placement, preview, clear, and cancel
- section plane and clipping boxes
- clipping helpers
- reload and close scene
- OpenGL shutdown without visible errors

## Adding Regression Tests

Add focused tests under the nearest module `tests` directory when possible.
Public facade behavior belongs in `tests/public_api_test.cpp`. Keep tests
deterministic, project-owned, and independent of external model files unless a
fixture is committed with license information.
