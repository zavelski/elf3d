# Module Map

Purpose: Record the actual Elf3D 0.1.0 CMake targets, responsibilities, and
dependency direction.

Applicable version: 0.1.0

Document status: Verified from CMake files and validation on 2026-06-23.

Last verified Git commit: `8504068`

Implementation source paths: `CMakeLists.txt`, `cmake/dependencies.cmake`,
`modules/*/CMakeLists.txt`, `modules/tools/*/CMakeLists.txt`,
`facade/elf3d/CMakeLists.txt`, `integrations/imgui/CMakeLists.txt`,
`apps/viewer/CMakeLists.txt`, `tests/CMakeLists.txt`

Known limitations: There is no install/package target and no CI workflow in the
verified repository state.

Related documents: `PUBLIC_API_OVERVIEW.md`, `RENDERING_PIPELINE.md`,
`TESTING.md`

## Top-Level Build

The root project is `Elf3D` version `0.1.0`. It requires CMake 3.25, C++20,
compiler extensions disabled, and MSVC dynamic runtime selection:

- Debug: `/MDd`
- Release: `/MD`

The supported presets are:

- `windows-debug`
- `windows-release`

Both presets use the Visual Studio 17 2022 x64 generator and write to
`out/build/windows-debug`, with Debug and Release separated by configuration.

## Target Map

| Target | Type | Responsibility | Public dependencies | Private dependencies |
| --- | --- | --- | --- | --- |
| `elf3d_core` | Static | Version data and core public headers. | none | none |
| `elf3d_math` | Static | GLM-backed internal math conventions and public value conversion. | `elf3d_core`, `glm::glm` | none |
| `elf3d_interaction` | Static | Viewport input transition and pointer capture state. | `elf3d_math`, `elf3d_core` | none |
| `elf3d_assets` | Static | Scene-owned meshes, images, textures, samplers, materials. | `elf3d_math`, `elf3d_core` | none |
| `elf3d_image` | Static | Bounded PNG/JPEG decode to RGBA8. | `elf3d_core` | `elf3d_third_party_stb` |
| `elf3d_scene` | Static | Entities, hierarchy, transforms, cameras, models, visibility, assets. | `elf3d_assets`, `elf3d_math`, `elf3d_core` | none |
| `elf3d_clipping` | Static | Neutral clipping filters, section planes, boxes, bounds math. | `elf3d_math`, `elf3d_core` | none |
| `elf3d_navigation` | Static | Orbit, pan, dolly, fit, reset, diagnostics. | `elf3d_interaction`, `elf3d_scene`, `elf3d_math`, `elf3d_core` | none |
| `elf3d_picking` | Static | CPU ray construction, broad phase, per-mesh BVH picking. | `elf3d_scene`, `elf3d_clipping`, `elf3d_assets`, `elf3d_math`, `elf3d_core` | none |
| `elf3d_tool_selection` | Static | Single-selection controller and pick-hit capture. | `elf3d_picking`, `elf3d_interaction`, `elf3d_scene`, `elf3d_math`, `elf3d_core` | none |
| `elf3d_tool_visibility` | Static | Per-viewport isolation and visibility-filter creation. | `elf3d_scene`, `elf3d_math`, `elf3d_core` | none |
| `elf3d_tool_measurement` | Static | Distance measurement anchors, snapshots, units, overlays. | `elf3d_picking`, `elf3d_clipping`, `elf3d_interaction`, `elf3d_scene`, `elf3d_assets`, `elf3d_math`, `elf3d_core` | none |
| `elf3d_tool_clipping` | Static | Viewport clipping state, visible clipped bounds, helper overlays. | `elf3d_clipping`, `elf3d_scene`, `elf3d_math`, `elf3d_core` | none |
| `elf3d_gltf` | Static | Private cgltf-based static glTF importer. | `elf3d_scene`, `elf3d_assets`, `elf3d_math`, `elf3d_core` | `elf3d_third_party_cgltf`, `elf3d_image` |
| `elf3d_graphics` | Static | Neutral device, render target, texture, mesh, pipeline interfaces. | `elf3d_clipping`, `elf3d_core` | none |
| `elf3d_backend_opengl` | Static | GLAD/OpenGL 4.1 implementation of graphics device. | `elf3d_graphics`, `elf3d_core` | `elf3d_third_party_glad` |
| `elf3d_renderer` | Static | Render-list preparation, PBR shader path, GPU caches, overlays. | `elf3d_scene`, `elf3d_clipping`, `elf3d_assets`, `elf3d_graphics`, `elf3d_math`, `elf3d_core` | none |
| `elf3d_viewport` | Static | Off-screen viewport state and render-target ownership. | tool modules, `elf3d_picking`, `elf3d_navigation`, `elf3d_interaction`, `elf3d_graphics`, `elf3d_renderer`, `elf3d_scene`, `elf3d_math`, `elf3d_core` | none |
| `elf3d` | Shared | Public DLL facade and composition root. | public include directory | all internal engine modules |
| `elf3d_imgui` | Static | Optional Dear ImGui texture/context helper. | `elf3d::elf3d`, `elf3d::third_party_imgui` | none |
| `elf3d_viewer` | Executable | Reference application and graphical testbed. | none | `elf3d::elf3d`, `elf3d::imgui` |

Third-party helper targets:

- `elf3d_third_party_glad`: checked-in generated GLAD OpenGL 4.1 core loader.
- `elf3d_third_party_cgltf`: cgltf implementation unit.
- `elf3d_third_party_stb`: stb image implementation unit.
- `elf3d_third_party_imgui`: Dear ImGui core and GLFW/OpenGL3 backends.
- `glfw`: FetchContent GLFW target.
- `glm::glm`: FetchContent GLM target.

## Plain-Text Dependency Graph

```text
elf3d_viewer
  -> elf3d_imgui
    -> elf3d
    -> elf3d_third_party_imgui -> glfw, OpenGL::GL
  -> elf3d
    -> elf3d_core
    -> elf3d_math -> glm::glm
    -> elf3d_graphics -> elf3d_clipping
    -> elf3d_backend_opengl -> elf3d_third_party_glad
    -> elf3d_assets
    -> elf3d_scene
    -> elf3d_interaction
    -> elf3d_navigation
    -> elf3d_picking
    -> elf3d_tool_selection
    -> elf3d_tool_visibility
    -> elf3d_tool_measurement
    -> elf3d_tool_clipping
    -> elf3d_gltf -> elf3d_image, elf3d_third_party_cgltf
    -> elf3d_renderer
    -> elf3d_viewport
```

## Confirmed Boundaries

- Dear ImGui and GLFW are limited to `elf3d_imgui`, third-party ImGui target,
  and `elf3d_viewer`.
- GLAD and raw OpenGL calls are limited to `elf3d_backend_opengl` and viewer
  final presentation code.
- GLM is internal to math and implementation modules; public headers expose
  project-owned value types.
- cgltf is private to `elf3d_gltf`.
- Scene does not depend on renderer.
- Renderer consumes scene and asset data but does not own logical scene state.

## Current Deviations or Gaps

- No install, export, or package configuration is present.
- No CI target or workflow is present.
- The public DLL is C++ ABI based; no C ABI or plugin ABI is implemented.
