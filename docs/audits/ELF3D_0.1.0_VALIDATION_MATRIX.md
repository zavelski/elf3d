# Elf3D 0.1.0 Validation Matrix

Date: 2026-06-23

Repository: `Z:/Elf3D`

Branch: `audit/0.1.0`

Validated commit before this document: `661d12f`

Inventory: `docs/audits/ELF3D_0.1.0_REPOSITORY_INVENTORY.md`

Audit: `docs/audits/ELF3D_0.1.0_AUDIT.md`

## Summary

Debug and Release configured, built, and passed CTest using the repository
presets and Visual Studio's bundled CMake/CTest executables. The viewer was
started non-interactively with the project-owned glTF fixture in both Debug and
Release and stayed alive for five seconds before being terminated by the smoke
script.

Goal 7 repeated clean Debug and Release validation, added public-header
self-containment checks, and verified release documentation paths/links.

Manual visual and interaction validation was not performed. Rendering
correctness, navigation, picking, selection, visibility, measurement, clipping,
normal user shutdown, and visual quality remain unverified by human inspection.

## Environment

- Host shell: PowerShell
- Repository root: `Z:/Elf3D`
- CMake executable:
  `C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe`
- CMake version: `3.31.6-msvc6`
- CTest executable:
  `C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ctest.exe`
- CTest version: `3.31.6-msvc6`
- Visual Studio: `17.14.23`
- Visual Studio installation version: `17.14.36811.4`
- MSBuild version from build output: `17.14.23+b0019275e`
- C compiler: MSVC `19.44.35222.0`
- C++ compiler: MSVC `19.44.35222.0`
- MSVC toolset path reported by CMake:
  `C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe`
- Windows SDK selected by CMake: `10.0.26100.0`
- CMake target platform reported by configure: Windows `10.0.26200`
- Generator: `Visual Studio 17 2022`
- Architecture: `x64`
- Binary directory: `Z:/Elf3D/out/build/windows-debug`

Note: `cmake`, `ctest`, and `cl` were not on `PATH` in the shell. Validation
used absolute paths for CMake and CTest. CMake located the MSVC compiler through
the Visual Studio generator.

## Configure and Build Matrix

| Area | Command | Result | Evidence |
| --- | --- | --- | --- |
| Debug configure | `cmake --fresh --preset windows-debug` | Passed | Generated build files in `out/build/windows-debug`; CMake reported MSVC 19.44.35222.0 and Windows SDK 10.0.26100.0. |
| Debug build | `cmake --build --preset windows-debug` | Passed | Built `elf3d.dll`, all internal libraries, all tests, `elf3d_imgui`, and `elf3d_viewer.exe`. |
| Debug warnings | Captured build output | None observed | MSBuild output contained target build lines and no warning diagnostics. |
| Release configure | `cmake --fresh --preset windows-release` | Passed | Generated build files in the preset-defined multi-config directory. |
| Release build | `cmake --build --preset windows-release` | Passed | Built Release `elf3d.dll`, all internal libraries, all tests, `elf3d_imgui`, and `elf3d_viewer.exe`. |
| Release warnings | Captured build output | None observed | MSBuild output contained target build lines and no warning diagnostics. |

`windows-debug` and `windows-release` both configure the same multi-config
Visual Studio build tree by design in `CMakePresets.json`.

## Automated Test Matrix

### Debug

Command:

```powershell
ctest --preset windows-debug --output-on-failure
```

Result: 16 passed, 0 failed, 0 skipped.

| Test | Result |
| --- | --- |
| `elf3d.math_conventions` | Passed |
| `elf3d.interaction` | Passed |
| `elf3d.assets` | Passed |
| `elf3d.image_decode` | Passed |
| `elf3d.scene` | Passed |
| `elf3d.clipping` | Passed |
| `elf3d.navigation` | Passed |
| `elf3d.picking` | Passed |
| `elf3d.selection` | Passed |
| `elf3d_tool_visibility_tests` | Passed |
| `elf3d_tool_measurement_tests` | Passed |
| `elf3d_tool_clipping_tests` | Passed |
| `elf3d.gltf_import` | Passed |
| `elf3d.renderer` | Passed |
| `elf3d.viewport_lifetime` | Passed |
| `elf3d.public_api_lifetime` | Passed |

### Release

Command:

```powershell
ctest --preset windows-release --output-on-failure
```

Result: 16 passed, 0 failed, 0 skipped.

| Test | Result |
| --- | --- |
| `elf3d.math_conventions` | Passed |
| `elf3d.interaction` | Passed |
| `elf3d.assets` | Passed |
| `elf3d.image_decode` | Passed |
| `elf3d.scene` | Passed |
| `elf3d.clipping` | Passed |
| `elf3d.navigation` | Passed |
| `elf3d.picking` | Passed |
| `elf3d.selection` | Passed |
| `elf3d_tool_visibility_tests` | Passed |
| `elf3d_tool_measurement_tests` | Passed |
| `elf3d_tool_clipping_tests` | Passed |
| `elf3d.gltf_import` | Passed |
| `elf3d.renderer` | Passed |
| `elf3d.viewport_lifetime` | Passed |
| `elf3d.public_api_lifetime` | Passed |

## Viewer Smoke Matrix

The viewer was launched with the project-owned glTF fixture:

```powershell
tests/fixtures/textured_pbr.gltf
```

| Configuration | Executable | Result | Limitations |
| --- | --- | --- | --- |
| Debug | `out/build/windows-debug/bin/Debug/elf3d_viewer.exe` | Process started and remained running for 5 seconds, then was terminated by the smoke script. | No visual inspection; no user-driven clean shutdown; no interaction validation. |
| Release | `out/build/windows-debug/bin/Release/elf3d_viewer.exe` | Process started and remained running for 5 seconds, then was terminated by the smoke script. | No visual inspection; no user-driven clean shutdown; no interaction validation. |

The smoke script intentionally terminated the viewer, so process exit code
`-1` is expected for this validation record and does not indicate a clean
viewer shutdown.

## Runtime Behavior Coverage

| Behavior | Status | Evidence |
| --- | --- | --- |
| Engine library compiles | Confirmed by compilation | Debug and Release `elf3d.dll` built. |
| Public API basic use | Confirmed by automated tests | `elf3d.public_api_lifetime` passed in Debug and Release. |
| Version API | Confirmed by automated tests | `elf3d.public_api_lifetime` checks `0.1.0`. |
| Math conventions | Confirmed by automated tests | `elf3d.math_conventions` passed. |
| Asset validation | Confirmed by automated tests | `elf3d.assets` passed. |
| Image decode | Confirmed by automated tests | `elf3d.image_decode` passed. |
| Scene hierarchy, visibility, bounds | Confirmed by automated tests | `elf3d.scene` passed. |
| Interaction click/drag state | Confirmed by automated tests | `elf3d.interaction` passed. |
| Orbit navigation | Confirmed by automated tests | `elf3d.navigation` passed. |
| CPU picking and BVH cache | Confirmed by automated tests | `elf3d.picking` passed. |
| Selection controller | Confirmed by automated tests | `elf3d.selection` passed. |
| Measurement anchors and units | Confirmed by automated tests | `elf3d_tool_measurement_tests` passed. |
| Clipping math and controller | Confirmed by automated tests | `elf3d.clipping` and `elf3d_tool_clipping_tests` passed. |
| Visibility isolation controller | Confirmed by automated tests | `elf3d_tool_visibility_tests` passed. |
| glTF static import subset | Confirmed by automated tests | `elf3d.gltf_import` passed. |
| Renderer preparation/caches | Confirmed by automated tests | `elf3d.renderer` passed. |
| Offscreen viewport lifetime with fakes | Confirmed by automated tests | `elf3d.viewport_lifetime` passed. |
| Viewer startup with fixture | Confirmed by runtime smoke test | Debug and Release process remained alive for 5 seconds. |
| Visual rendering correctness | Not verified | No screenshot or human inspection was performed. |
| Navigation interaction in viewer | Not verified | No manual or automated UI interaction was performed. |
| Picking and selection in viewer | Not verified | No manual or automated UI interaction was performed. |
| Measurement interaction in viewer | Not verified | No manual or automated UI interaction was performed. |
| Clipping interaction in viewer | Not verified | No manual or automated UI interaction was performed. |
| Normal viewer shutdown | Not verified | Smoke script terminated the process instead of closing the UI. |
| External user model loading | Not verified | Only project-owned fixture startup was smoke-tested. |

## Static Runtime Inspection

The following items were confirmed by source inspection during the audit:

- The viewer initializes GLFW, creates and makes current an OpenGL 4.1 core
  context, creates the Elf3D engine, creates the viewport, then initializes the
  ImGui integration.
- The viewer owns the native window, operating-system event loop, Dear ImGui
  context, GUI construction, final frame presentation, and swap.
- Input is translated from ImGui/GLFW into public `ViewportInput` snapshots
  before entering the engine.
- Scene replacement loads into a newly created scene and preserves the current
  scene on load failure.
- Viewport resize and render paths go through the public `Viewport` facade and
  internal `OffscreenViewport`.
- Renderer and picking caches are released when a scene is destroyed under the
  documented Engine-outlives-Scene contract.
- OpenGL resource deletion requires the owning graphics thread and current
  compatible context; the backend skips deletion if those preconditions are not
  met.

## Validation Gaps

- Manual visual inspection of `elf3d_viewer` was not performed.
- No screenshot or pixel-readback validation was performed.
- No CI workflow was found or executed.
- No benchmark or performance measurement was performed.
- No external model corpus was used.

## Goal 7 Release-Candidate Validation Addendum

Date: 2026-06-23

Implementation commit before release snapshot: `79fd4bc`

Additional commands executed:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --fresh --preset windows-debug
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build --preset windows-debug
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --preset windows-debug --output-on-failure
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --fresh --preset windows-release
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build --preset windows-release
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --preset windows-release --output-on-failure
```

Goal 7 results:

| Area | Result |
| --- | --- |
| Debug configure/build | Passed, no warning diagnostics observed. |
| Debug CTest | Passed 16/16. |
| Release configure/build | Passed, no warning diagnostics observed. |
| Release CTest | Passed 16/16. |
| Debug viewer smoke | Process stayed alive five seconds with `tests/fixtures/textured_pbr.gltf`, then was intentionally terminated. |
| Release viewer smoke | Process stayed alive five seconds with `tests/fixtures/textured_pbr.gltf`, then was intentionally terminated. |
| Public headers | All public headers under `include/elf3d` compiled individually as forced includes with MSVC C++20, `/permissive-`, `/W4`, `/WX`. |
| Documentation paths and links | 43 required paths existed; Markdown links in 31 Markdown files resolved. |

Release decision: `Not ready due to release blockers`.

Remaining release blocker: manual visual viewer validation has not been
performed.

## Commands Executed

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --version
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --version
& 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe' -latest -products * -property catalog_productDisplayVersion
& 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe' -latest -products * -property installationVersion
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --fresh --preset windows-debug
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build --preset windows-debug
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --preset windows-debug --output-on-failure
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --fresh --preset windows-release
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build --preset windows-release
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe' --preset windows-release --output-on-failure
```

Viewer smoke command pattern:

```powershell
$viewer = Resolve-Path 'out\build\windows-debug\bin\<Config>\elf3d_viewer.exe'
$fixture = Resolve-Path 'tests\fixtures\textured_pbr.gltf'
$process = Start-Process -FilePath $viewer -ArgumentList @($fixture) -PassThru
Start-Sleep -Seconds 5
if ($process.HasExited) {
    Write-Output "viewer_exited=$($process.ExitCode)"
} else {
    Write-Output "viewer_started_and_remained_running=true pid=$($process.Id)"
    Stop-Process -Id $process.Id
    $process.WaitForExit()
    Write-Output "viewer_terminated_for_smoke_test=true exit=$($process.ExitCode)"
}
```
