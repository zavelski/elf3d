# Module Map

Purpose: Record the actual Elf3D 0.3.0 CMake targets, responsibilities,
dependency direction, and current C++20 named-module migration state.

Applicable version: 0.3.0

Document status: Living build/module map for the
`feature/cpp20-named-modules-object-dll-migration` branch.

Last verified Git commit: pending migration commit

Implementation source paths: `CMakeLists.txt`, `cmake/dependencies.cmake`,
`cmake/compiler_options.cmake`, `modules/*/CMakeLists.txt`,
`modules/tools/*/CMakeLists.txt`, `facade/elf3d/CMakeLists.txt`,
`integrations/imgui/CMakeLists.txt`, `apps/viewer/CMakeLists.txt`,
`tests/CMakeLists.txt`

Known limitations: There is no install/export SDK target. Windows viewer ZIP
packaging is script-based. Legacy internal include paths remain as import-only
shims for source compatibility during the named-module migration.

Related documents: `PUBLIC_API_OVERVIEW.md`, `RENDERING_PIPELINE.md`,
`TESTING.md`, root `ARCHITECTURE.md`, root `CODING_POLICY.md`

## Top-Level Build

The root project is `Elf3D` version `0.3.0`. It requires CMake 3.28 or newer,
C++20, compiler extensions disabled, and MSVC dynamic runtime selection:

- Debug: `/MDd`
- Release: `/MD`

The current validated local toolchain is:

- Visual Studio 2022 v17.14.35
- MSVC 19.44.35228.0
- CMake/CTest 3.31.6-msvc6 from Visual Studio bundled tools
- Generator: `Visual Studio 17 2022`
- Architecture: `x64`

The supported presets are:

- `windows-debug`
- `windows-release`

Both presets use the Visual Studio 17 2022 x64 generator and write to
`out/build/windows-debug` or `out/build/windows-release`, with Debug and
Release separated by configuration.

## C++20 Named Modules

Project-owned C++20 named modules define logical internal architecture for each
current internal engine OBJECT library. These interfaces are internal
implementation artifacts; public consumers still use `include/elf3d`.

| Named module | Interface file | Implementation | Notes |
| --- | --- | --- | --- |
| `elf.core` | `modules/core/src/elf.core.cppm` | `modules/core/src/version_data.cpp` | Exposes internal version data declarations used by the public facade. |
| `elf.math` | `modules/math/src/elf.math.cppm` | `modules/math/src/conventions.cpp` | Exposes GLM-neutral math helpers; GLM aliases and conversions remain implementation-only details. |
| `elf.interaction` | `modules/interaction/src/elf.interaction.cppm` | `modules/interaction/src/viewport_interaction.cpp` | Exposes viewport pointer and drag transition state. |
| `elf.assets` | `modules/assets/src/elf.assets.cppm` | `modules/assets/src/storage.cpp` | Exposes scene-owned asset records, asset storage, and internal handle access. |
| `elf.image` | `modules/image/src/elf.image.cppm` | `modules/image/src/image_decoder.cpp` | Exposes bounded PNG/JPEG decoding without stb types. |
| `elf.scene` | `modules/scene/src/elf.scene.cppm` | `modules/scene/src/storage.cpp`, `modules/scene/src/import_builder.cpp`, `modules/scene/src/scene.cpp` | Exposes GLM-neutral scene storage, import building, access, and visibility declarations. |
| `elf.clipping` | `modules/clipping/src/elf.clipping.cppm` | `modules/clipping/src/filter.cpp` | Exposes clipping filter data and GLM-neutral filter algorithms. |
| `elf.navigation` | `modules/navigation/src/elf.navigation.cppm` | `modules/navigation/src/orbit_navigation.cpp` | Exposes orbit navigation controller declarations. |
| `elf.picking` | `modules/picking/src/elf.picking.cppm` | `modules/picking/src/service.cpp` | Exposes ray, triangle, BVH picking declarations, and picking service. |
| `elf.tool.selection` | `modules/tools/selection/src/elf.tool.selection.cppm` | `modules/tools/selection/src/selection_controller.cpp` | Exposes the selection controller. |
| `elf.tool.visibility` | `modules/tools/visibility/src/elf.tool.visibility.cppm` | `modules/tools/visibility/src/visibility_controller.cpp` | Exposes the visibility/isolation controller. |
| `elf.tool.measurement` | `modules/tools/measurement/src/elf.tool.measurement.cppm` | `modules/tools/measurement/src/distance_measurement.cpp` | Exposes distance measurement controller and overlay data. |
| `elf.tool.clipping` | `modules/tools/clipping/src/elf.tool.clipping.cppm` | `modules/tools/clipping/src/clipping_controller.cpp` | Exposes clipping controller and helper overlay data. |
| `elf.gltf` | `modules/gltf/src/elf.gltf.cppm` | `modules/gltf/src/importer.cpp` | Exposes the validated importer entry point without cgltf types. |
| `elf.graphics` | `modules/graphics/src/elf.graphics.cppm` | `modules/graphics/src/device.cpp` | Exposes backend-neutral graphics device, target, mesh, texture, and pipeline interfaces. |
| `elf.backend.opengl` | `modules/backend_opengl/src/elf.backend.opengl.cppm` | `modules/backend_opengl/src/device.cpp` | Exposes the OpenGL device factory without GLAD or raw OpenGL types. |
| `elf.renderer` | `modules/renderer/src/elf.renderer.cppm` | `modules/renderer/src/renderer.cpp` | Exposes renderer, render-list preparation, and GPU picking result declarations without GLM types. |
| `elf.viewport` | `modules/viewport/src/elf.viewport.cppm` | `modules/viewport/src/offscreen_viewport.cpp` | Exposes the off-screen viewport orchestration type. |

Module policy:

- dotted names such as `elf.core` and `elf.math` are naming conventions, not
  language-level nesting;
- `import elf.core;` does not imply any submodule import;
- module export is separate from DLL symbol export;
- public DLL symbols remain controlled by `ELF3D_API`;
- third-party headers and types must not be exported from project module
  interfaces;
- module partitions, private module fragments, header units, and mandatory
  `import std` are not used;
- ordinary standard-library `#include` use remains the default;
- generated BMI, IFC, PCM, GCM, and similar artifacts are build outputs.

## Target Map

Internal engine targets are CMake OBJECT libraries linked into the final
`elf3d` shared library. Tests that exercise an internal module include the
needed object files explicitly through `elf3d_link_object_libraries`.

| Target | Type | Responsibility | Public dependencies | Private dependencies |
| --- | --- | --- | --- | --- |
| `elf3d_core` | Object | Version data and core internal module declarations. | none | none |
| `elf3d_math` | Object | GLM-backed internal math conventions and public value conversion. | `elf3d_core`, `glm::glm` | none |
| `elf3d_interaction` | Object | Viewport input transition and pointer capture state for click, orbit, pan, and zoom drags. | `elf3d_math`, `elf3d_core` | none |
| `elf3d_assets` | Object | Scene-owned meshes, images, textures, samplers, materials. | `elf3d_math`, `elf3d_core` | none |
| `elf3d_image` | Object | Bounded PNG/JPEG decode to RGBA8. | `elf3d_core` | `elf3d_third_party_stb` |
| `elf3d_scene` | Object | Entities, hierarchy, transforms, cameras, models, visibility, assets. | `elf3d_assets`, `elf3d_math`, `elf3d_core` | none |
| `elf3d_clipping` | Object | Neutral clipping filters, section planes, boxes, bounds math. | `elf3d_math`, `elf3d_core` | none |
| `elf3d_navigation` | Object | Orbit, pan, dolly, fit, reset, diagnostics. | `elf3d_interaction`, `elf3d_scene`, `elf3d_math`, `elf3d_core` | none |
| `elf3d_picking` | Object | CPU ray construction, broad phase, per-mesh BVH picking. | `elf3d_scene`, `elf3d_clipping`, `elf3d_assets`, `elf3d_math`, `elf3d_core` | none |
| `elf3d_tool_selection` | Object | Single-selection controller and pick-hit capture. | `elf3d_picking`, `elf3d_interaction`, `elf3d_scene`, `elf3d_math`, `elf3d_core` | none |
| `elf3d_tool_visibility` | Object | Per-viewport isolation and visibility-filter creation. | `elf3d_scene`, `elf3d_math`, `elf3d_core` | none |
| `elf3d_tool_measurement` | Object | Distance measurement anchors, snapshots, units, overlays. | `elf3d_picking`, `elf3d_clipping`, `elf3d_interaction`, `elf3d_scene`, `elf3d_assets`, `elf3d_math`, `elf3d_core` | none |
| `elf3d_tool_clipping` | Object | Viewport clipping state, visible clipped bounds, helper overlays. | `elf3d_clipping`, `elf3d_scene`, `elf3d_math`, `elf3d_core` | none |
| `elf3d_gltf` | Object | Private cgltf-based static glTF importer. | `elf3d_scene`, `elf3d_assets`, `elf3d_math`, `elf3d_core` | `elf3d_third_party_cgltf`, `elf3d_image` |
| `elf3d_graphics` | Object | Neutral device, render target, picking target, texture, mesh, pipeline interfaces. | `elf3d_clipping`, `elf3d_core` | none |
| `elf3d_backend_opengl` | Object | GLAD/OpenGL 4.1 implementation of graphics device, render targets, and integer picking targets. | `elf3d_graphics`, `elf3d_core` | `elf3d_third_party_glad` |
| `elf3d_renderer` | Object | Render-list preparation, PBR shader path, GPU picking pass, GPU caches, overlays. | `elf3d_scene`, `elf3d_clipping`, `elf3d_assets`, `elf3d_graphics`, `elf3d_math`, `elf3d_core` | none |
| `elf3d_viewport` | Object | Off-screen viewport state, render-target ownership, picking-target ownership, and tool orchestration. | tool modules, `elf3d_picking`, `elf3d_navigation`, `elf3d_interaction`, `elf3d_graphics`, `elf3d_renderer`, `elf3d_scene`, `elf3d_math`, `elf3d_core` | none |
| `elf3d` | Shared | Public DLL facade and composition root. | public include directory | all internal engine OBJECT modules |
| `elf3d_imgui` | Static | Optional Dear ImGui texture/context helper. | `elf3d::elf3d`, `elf3d::third_party_imgui` | none |
| `elf3d_viewer` | Executable | Reference application, graphical testbed, and packaged UI asset host. | none | `elf3d::elf3d`, `elf3d::imgui`, Windows WIC system libraries on Windows |

Third-party helper targets:

- `elf3d_third_party_glad`: checked-in generated GLAD OpenGL 4.1 core loader.
- `elf3d_third_party_cgltf`: cgltf implementation unit.
- `elf3d_third_party_stb`: stb image implementation unit.
- `elf3d_third_party_imgui`: Dear ImGui core and GLFW/OpenGL3 backends.
- `glfw`: FetchContent GLFW target.
- `glm::glm`: FetchContent GLM target.

Third-party targets remain ordinary library/header targets. They are not
converted to C++ named modules.

## Plain-Text Dependency Graph

```text
elf3d_viewer
  -> elf3d_imgui
    -> elf3d
    -> elf3d_third_party_imgui -> glfw, OpenGL::GL
  -> Windows WIC system libraries on Windows
  -> elf3d
    -> object code from elf3d_core
    -> object code from elf3d_math -> glm::glm
    -> object code from elf3d_graphics -> elf3d_clipping
    -> object code from elf3d_backend_opengl -> elf3d_third_party_glad
    -> object code from elf3d_assets
    -> object code from elf3d_image -> elf3d_third_party_stb
    -> object code from elf3d_scene
    -> object code from elf3d_interaction
    -> object code from elf3d_navigation
    -> object code from elf3d_picking
    -> object code from elf3d_tool_selection
    -> object code from elf3d_tool_visibility
    -> object code from elf3d_tool_measurement
    -> object code from elf3d_tool_clipping
    -> object code from elf3d_gltf -> elf3d_third_party_cgltf
    -> object code from elf3d_renderer
    -> object code from elf3d_viewport
```

## Confirmed Boundaries

- Dear ImGui and GLFW are limited to `elf3d_imgui`, third-party ImGui target,
  and `elf3d_viewer`.
- GLAD and raw OpenGL calls are limited to `elf3d_backend_opengl`, viewer final
  presentation code, and viewer-owned toolbar icon texture upload.
- Windows WIC is used only by `elf3d_viewer` on Windows to decode packaged
  toolbar PNG assets.
- GLM is internal to math and implementation modules; public headers expose
  project-owned value types.
- cgltf is private to `elf3d_gltf`.
- Scene does not depend on renderer.
- Renderer consumes scene and asset data but does not own logical scene state.

## Current Deviations or Gaps

- Named-module declarations are owned by real module interface units for every
  built-in engine OBJECT library.
- Module implementation units are present for every built-in engine OBJECT
  library. Internal include paths remain only as import shims while existing
  source files migrate to direct imports.
- GLM-backed aliases and conversions are implementation details under the math
  detail include path; exported named-module surfaces use project-owned public
  value types instead of GLM types.
- No install/export SDK package configuration is present. Windows viewer ZIP
  packaging is script-based and includes required runtime assets.
- The public DLL is C++ ABI based; no C ABI or plugin ABI is implemented.
