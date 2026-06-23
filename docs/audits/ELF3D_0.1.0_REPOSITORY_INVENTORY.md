# Elf3D 0.1.0 Repository Inventory

## Purpose

This document records the repository state preserved before the Elf3D 0.1.0
audit work. It is an inventory only; it does not classify correctness,
architecture conformance, or release readiness.

## Repository Identity

- Repository root: `Z:/Elf3D`
- Active development branch before audit branch creation: `develop`
- Pre-audit checkpoint commit: `f8fe3a827bc81dadb461e58bdbe846958dab346a`
- Checkpoint commit message: `chore: checkpoint Elf3D 0.1.0 before audit`
- Audit branch: `audit/0.1.0`
- Audit branch base: `f8fe3a827bc81dadb461e58bdbe846958dab346a`
- Previous develop commit: `a19f284afc91077bc290d75f476f3c7f6337b520`
- Existing tags at inventory time: none
- Configured remotes at inventory time: none
- Git safe-directory note: Git initially refused the repository as dubious on
  `Z:/Elf3D`; `Z:/Elf3D` was added to the global Git safe-directory list so the
  required inventory commands could run.

## Working Tree State

Before the checkpoint, `develop` contained a large uncommitted implementation
slice across public headers, internal modules, tests, viewer code, dependency
records, and license files. Ignored build and local runtime artifacts were
present under `out/` and as `imgui.ini`; they were not staged.

The checkpoint commit added or preserved 110 repository files and left the
working tree clean. The audit branch was created after that clean checkpoint.

## Repository Layout

Top-level project-owned layout:

```text
apps/
cmake/
docs/
facade/
include/
integrations/
modules/
tests/
third_party/
AGENTS.md
ARCHITECTURE.md
CMakeLists.txt
CMakePresets.json
CODING_POLICY.md
README.md
THIRD_PARTY.md
```

Ignored or generated local artifacts observed:

```text
imgui.ini
out/
```

## Documentation Present

Project documentation present at inventory time:

- `AGENTS.md`
- `ARCHITECTURE.md`
- `CODING_POLICY.md`
- `README.md`
- `THIRD_PARTY.md`

Third-party license files present:

- `third_party/licenses/cgltf-LICENSE.txt`
- `third_party/licenses/glad-LICENSE.txt`
- `third_party/licenses/glfw-LICENSE.md`
- `third_party/licenses/glm-copying.txt`
- `third_party/licenses/imgui-LICENSE.txt`
- `third_party/licenses/stb-LICENSE.txt`

`PROJECT_STATE_EN.md` was not found anywhere under `Z:/Elf3D`, including
imported variants matching `PROJECT_STATE*`. No normalization could be
performed during Goal 1 because there was no source document to rename.

## Public Include Directories

Public facade include root:

- `include/elf3d`

Public headers under the facade include root:

- `include/elf3d/assets.h`
- `include/elf3d/clipping.h`
- `include/elf3d/core/api.h`
- `include/elf3d/core/error.h`
- `include/elf3d/core/result.h`
- `include/elf3d/core/version.h`
- `include/elf3d/elf3d.h`
- `include/elf3d/graphics.h`
- `include/elf3d/math/value_types.h`
- `include/elf3d/measurement.h`
- `include/elf3d/navigation.h`
- `include/elf3d/picking.h`
- `include/elf3d/scene.h`
- `include/elf3d/selection.h`
- `include/elf3d/viewport.h`

Public or module-facing include roots are also declared by internal targets
under `modules/*/include` and by `integrations/imgui/include`.

## Source and Module Directories

Engine and integration source directories:

- `facade/elf3d`
- `modules/core`
- `modules/math`
- `modules/interaction`
- `modules/assets`
- `modules/image`
- `modules/scene`
- `modules/clipping`
- `modules/navigation`
- `modules/picking`
- `modules/tools/selection`
- `modules/tools/visibility`
- `modules/tools/measurement`
- `modules/tools/clipping`
- `modules/gltf`
- `modules/graphics`
- `modules/backend_opengl`
- `modules/renderer`
- `modules/viewport`
- `integrations/imgui`
- `apps/viewer`

## CMake Presets

Configured presets from `CMakePresets.json`:

- Configure preset `windows-debug`
  - Generator: `Visual Studio 17 2022`
  - Architecture: `x64`
  - Binary directory: `out/build/windows-debug`
  - Cache: `BUILD_TESTING=ON`, `CMAKE_CONFIGURATION_TYPES=Debug;Release`,
    `ELF3D_BUILD_VIEWER=ON`
- Configure preset `windows-release`
  - Generator: `Visual Studio 17 2022`
  - Architecture: `x64`
  - Binary directory: `out/build/windows-debug`
  - Cache: `BUILD_TESTING=ON`, `CMAKE_CONFIGURATION_TYPES=Debug;Release`,
    `ELF3D_BUILD_VIEWER=ON`
- Build preset `windows-debug`
  - Configuration: `Debug`
- Build preset `windows-release`
  - Configuration: `Release`
- Test preset `windows-debug`
  - Configuration: `Debug`
  - Output on failure: enabled
- Test preset `windows-release`
  - Configuration: `Release`
  - Output on failure: enabled

Both configure presets intentionally point at the same Visual Studio
multi-config build tree.

## Build Configuration

Root CMake declarations:

- Minimum CMake version: `3.25`
- Project: `Elf3D`
- Project version: `0.1.0`
- Languages: C and C++
- C++ standard: C++20
- C++ extensions: disabled
- MSVC runtime: `MultiThreaded$<$<CONFIG:Debug>:Debug>DLL`
- Runtime output: `${CMAKE_BINARY_DIR}/bin/$<CONFIG>`
- Library output: `${CMAKE_BINARY_DIR}/bin/$<CONFIG>`
- Archive output: `${CMAKE_BINARY_DIR}/lib/$<CONFIG>`
- Viewer option: `ELF3D_BUILD_VIEWER` default `ON`
- Testing: enabled through `include(CTest)` and `BUILD_TESTING`

## Primary CMake Targets

Engine and integration targets:

| Target | Type | Source location | Primary dependencies |
| --- | --- | --- | --- |
| `elf3d_core` | static library | `modules/core` | none above public include root |
| `elf3d_math` | static library | `modules/math` | `elf3d_core`, `glm::glm` |
| `elf3d_interaction` | static library | `modules/interaction` | `elf3d_math`, `elf3d_core` |
| `elf3d_assets` | static library | `modules/assets` | `elf3d_math`, `elf3d_core` |
| `elf3d_image` | static library | `modules/image` | `elf3d_core`, private `elf3d_third_party_stb` |
| `elf3d_scene` | static library | `modules/scene` | `elf3d_assets`, `elf3d_math`, `elf3d_core` |
| `elf3d_clipping` | static library | `modules/clipping` | `elf3d_math`, `elf3d_core` |
| `elf3d_navigation` | static library | `modules/navigation` | `elf3d_interaction`, `elf3d_scene`, `elf3d_math`, `elf3d_core` |
| `elf3d_picking` | static library | `modules/picking` | `elf3d_scene`, `elf3d_clipping`, `elf3d_assets`, `elf3d_math`, `elf3d_core` |
| `elf3d_tool_selection` | static library | `modules/tools/selection` | `elf3d_picking`, `elf3d_interaction`, `elf3d_scene`, `elf3d_math`, `elf3d_core` |
| `elf3d_tool_visibility` | static library | `modules/tools/visibility` | `elf3d_scene`, `elf3d_math`, `elf3d_core` |
| `elf3d_tool_measurement` | static library | `modules/tools/measurement` | `elf3d_picking`, `elf3d_clipping`, `elf3d_interaction`, `elf3d_scene`, `elf3d_assets`, `elf3d_math`, `elf3d_core` |
| `elf3d_tool_clipping` | static library | `modules/tools/clipping` | `elf3d_clipping`, `elf3d_scene`, `elf3d_math`, `elf3d_core` |
| `elf3d_gltf` | static library | `modules/gltf` | `elf3d_scene`, `elf3d_assets`, `elf3d_math`, `elf3d_core`, private `elf3d_third_party_cgltf`, private `elf3d_image` |
| `elf3d_graphics` | static library | `modules/graphics` | `elf3d_clipping`, `elf3d_core` |
| `elf3d_backend_opengl` | static library | `modules/backend_opengl` | `elf3d_graphics`, `elf3d_core`, private `elf3d_third_party_glad` |
| `elf3d_renderer` | static library | `modules/renderer` | `elf3d_scene`, `elf3d_clipping`, `elf3d_assets`, `elf3d_graphics`, `elf3d_math`, `elf3d_core` |
| `elf3d_viewport` | static library | `modules/viewport` | tools, picking, navigation, interaction, graphics, renderer, scene, math, core |
| `elf3d` | shared library | `facade/elf3d` | internal static libraries listed above |
| `elf3d_imgui` | static library | `integrations/imgui` | `elf3d::elf3d`, `elf3d::third_party_imgui` |
| `elf3d_viewer` | executable | `apps/viewer` | `elf3d::elf3d`, `elf3d::imgui` |

Third-party and generated-support targets:

| Target | Type | Source |
| --- | --- | --- |
| `elf3d_third_party_glad` | static library | checked-in `third_party/glad` |
| `elf3d_third_party_imgui` | static library | FetchContent Dear ImGui sources |
| `elf3d_third_party_cgltf` | static library | private cgltf implementation translation unit |
| `elf3d_third_party_stb` | static library | private stb_image implementation translation unit |
| `glfw` | third-party target | FetchContent GLFW |
| `glm::glm` | third-party target | FetchContent GLM |

The public shared-library product is `elf3d`. On Windows the expected runtime
artifact is `elf3d.dll`.

## Test Targets

CTest-registered tests declared by CMake:

| Test name | Executable | Source location |
| --- | --- | --- |
| `elf3d.public_api_lifetime` | `elf3d_public_api_test` | `tests` |
| `elf3d.math_conventions` | `elf3d_math_test` | `modules/math/tests` |
| `elf3d.assets` | `elf3d_assets_test` | `modules/assets/tests` |
| `elf3d.image_decode` | `elf3d_image_test` | `modules/image/tests` |
| `elf3d.scene` | `elf3d_scene_test` | `modules/scene/tests` |
| `elf3d.clipping` | `elf3d_clipping_test` | `modules/clipping/tests` |
| `elf3d.interaction` | `elf3d_interaction_test` | `modules/interaction/tests` |
| `elf3d.navigation` | `elf3d_navigation_test` | `modules/navigation/tests` |
| `elf3d.picking` | `elf3d_picking_test` | `modules/picking/tests` |
| `elf3d.selection` | `elf3d_selection_test` | `modules/tools/selection/tests` |
| `elf3d_tool_visibility_tests` | `elf3d_tool_visibility_tests` | `modules/tools/visibility/tests` |
| `elf3d_tool_measurement_tests` | `elf3d_tool_measurement_tests` | `modules/tools/measurement/tests` |
| `elf3d_tool_clipping_tests` | `elf3d_tool_clipping_tests` | `modules/tools/clipping/tests` |
| `elf3d.gltf_import` | `elf3d_gltf_test` | `modules/gltf/tests` |
| `elf3d.renderer` | `elf3d_renderer_test` | `modules/renderer/tests` |
| `elf3d.viewport_lifetime` | `elf3d_viewport_test` | `modules/viewport/tests` |

Project-owned fixture observed:

- `tests/fixtures/textured_pbr.gltf`

## Third-Party Dependencies

Dependencies recorded in `THIRD_PARTY.md` and CMake:

| Dependency | Source | Pinned revision | Integration |
| --- | --- | --- | --- |
| Dear ImGui | `https://github.com/ocornut/imgui.git`, `docking` | `036bf939b6f8d74ad76bcf926b757c56e68c54ff` | FetchContent, `elf3d_third_party_imgui` |
| GLFW | `https://github.com/glfw/glfw.git`, release `3.4` | `a74efa0d5628b74adc0426af4c5710e287fa7c2c` | FetchContent |
| GLM | `https://github.com/g-truc/glm.git`, release `1.0.3` | `8d1fd52e5ab5590e2c81768ace50c72bae28f2ed` | FetchContent exact archive with SHA-256 |
| GLAD | `https://github.com/Dav1dde/glad.git`, release `v2.0.8` | `73db193f853e2ee079bf3ca8a64aa2eaf6459043` | generated OpenGL 4.1 core loader checked into `third_party/glad` |
| cgltf | `https://github.com/jkuhlmann/cgltf.git`, release `v1.15` | `360db1a95480fe102ae9c69b27c5d101167ff5ba` | FetchContent, `elf3d_third_party_cgltf` |
| stb | `https://github.com/nothings/stb.git` | `31c1ad37456438565541f4919958214b6e762fb4` | FetchContent, `elf3d_third_party_stb` |

No Git submodules were configured.

## Version Declarations

Observed version declarations:

- `CMakeLists.txt`: project version `0.1.0`
- `modules/core/src/version_data.cpp`: `VersionData{0, 1, 0}`
- `modules/core/src/version_data.cpp`: version string `"0.1.0"`
- `facade/elf3d/src/engine.cpp`: public `elf3d::version()` and
  `elf3d::version_string()` forward to core version data
- `apps/viewer/src/main.cpp`: About/status UI displays
  `elf3d::version_string()`

## Known Build Entry Points

Documented build commands:

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug

cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release
```

Documented Debug viewer command:

```powershell
.\out\build\windows-debug\bin\Debug\elf3d_viewer.exe
```

## Checkpoint Validation Attempt

After the code-changing checkpoint commit, the following command was attempted:

```powershell
cmake --preset windows-debug
```

Result: not executed because `cmake` was not available on `PATH` in the current
shell. `Get-Command cmake -All` and `where.exe cmake` also failed to locate a
CMake executable. The bundled Codex workspace dependency list exposed Git,
Python, Node, pnpm, and document/PDF helper binaries, but no CMake executable.

No compile, CTest, or viewer validation was performed during Goal 1.

## Repository Anomalies

- `PROJECT_STATE_EN.md` is absent, so the requested comparison baseline is not
  present in the repository at inventory time.
- No Git remote is configured.
- No Git tags are present.
- Local ignored build and validation artifacts exist under `out/`.
- Local ignored Dear ImGui state exists as `imgui.ini`.
- CMake is not available on the current shell `PATH`, blocking immediate
  configure/build/test validation from this environment.
