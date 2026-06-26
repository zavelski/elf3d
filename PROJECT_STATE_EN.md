# Elf3D 0.3.0 Project State

Purpose: Living project-state baseline for the current 0.3.0 development line.

Applicable version: 0.3.0

Document status: Living project-state document.

Last verified implementation commit: pending C++20 module migration commit

Implementation source paths: `include/elf3d`, `modules`, `facade/elf3d`,
`integrations/imgui`, `apps/viewer`, `tests`, `CMakeLists.txt`,
`CMakePresets.json`, `README.md`, `LICENSE`, `THIRD_PARTY.md`, `.github`,
`scripts`

Known limitations: Release records for 0.2.0 are tracked under
`docs/releases/0.2.0/`. Historical 0.1.0 records remain immutable under
`docs/releases/0.1.0/`.

Related documents: `docs/README.md`, `docs/MODULE_MAP.md`, `docs/TESTING.md`,
`docs/releases/0.2.0/RELEASE_CHECKLIST.md`,
`docs/releases/0.2.0/VALIDATION_SUMMARY.md`

## Repository State

- Product: portable C++20 3D visualization engine
- Public library: `elf3d.dll`
- Optional integration: `elf3d_imgui`
- Reference app: `elf3d_viewer`
- Active migration branch: `feature/cpp20-named-modules-object-dll-migration`
- Previous public release tag: `v0.2.0`
- Current release target: `v0.3.0`
- Primary validated local toolchain: Visual Studio 2022 v17.14.35, MSVC
  19.44.35228.0, CMake 3.31.6-msvc6

## Implemented Vertical Slice

Elf3D 0.3.0 implements:

- public `Engine`, `Scene`, `Viewport`, and `SceneHierarchySnapshot` facades
- version API returning `0.3.0`
- one public `elf3d` DLL assembled from internal engine OBJECT libraries
- C++20 named-module interface for every internal engine OBJECT library, with
  `elf.core` already using a module implementation unit for version data
- scene entities, hierarchy, transforms, explicit local matrices, cameras,
  models, persistent visibility, bounds, hierarchy snapshots, and statistics
- scene-owned CPU mesh, image, texture, sampler, and material assets
- private glTF/GLB importer for bounded static triangle geometry
- private PNG/JPEG decode to RGBA8
- OpenGL 4.1 off-screen viewport rendering
- opaque metallic-roughness directional-light shader path
- GPU mesh and texture caches
- GPU-first viewport picking with CPU triangle refinement and CPU BVH fallback
- viewport orbit/pan/zoom navigation, dynamic examine pivot, fit, and reset
- single selection per viewport
- scene visibility and viewport isolation
- one distance measurement per viewport
- one section plane and up to three axis-aligned clipping boxes per viewport
- Dear ImGui/GLFW reference viewer with docking, Droid Sans UI font asset,
  light Low.3D-inspired panels, generated PNG toolbar icons, command toolbar,
  Blender-like Open File modal, and Windows GUI-subsystem startup

## Architecture Boundaries

Confirmed boundaries:

- `elf3d` does not depend on Dear ImGui or GLFW.
- Dear ImGui and GLFW are limited to `elf3d_imgui`, its third-party target, and
  `elf3d_viewer`.
- OpenGL and GLAD are isolated in `elf3d_backend_opengl` and viewer final
  presentation code.
- Viewer PNG toolbar decoding uses Windows WIC only inside `elf3d_viewer`.
- GLM and cgltf do not appear in public Elf3D headers.
- Scene does not depend on renderer.
- Renderer consumes scene and asset data but does not own logical scene state.
- Internal engine build groups are OBJECT libraries linked into `elf3d`; static
  libraries are kept for third-party helpers and the optional ImGui integration.
- C++ named-module export is separate from DLL symbol export; public symbols
  remain controlled through `ELF3D_API`.

## Validation State

0.2.0 public-release validation is recorded in `docs/releases/0.2.0/`.
0.3.0 validation must be refreshed from the current development commit before a
release.

Not yet validated:

- performance benchmark metrics
- external model corpus

## Known Limitations

- C++ DLL ABI requires compatible compiler/standard library/runtime.
- No stable C ABI.
- No runtime plugin ABI.
- Most named modules still use compatibility headers and ordinary implementation
  translation units during the incremental migration.
- Only OpenGL 4.1 backend.
- Opaque rendering only.
- glTF alpha factor, alpha mask, and alpha blend are not rendered.
- No animations, skins, morphs, compression extensions, KTX2, cameras, lights,
  normal maps, occlusion, or emissive material support.
- One selected entity and one distance measurement per viewport.
- Clipping boxes are axis aligned.
- Scene mutation and rendering are single-threaded.
