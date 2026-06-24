# Elf3D 0.2.0 Project State

Purpose: Living project-state baseline for the current 0.2.0 release.

Applicable version: 0.2.0

Document status: Living project-state document.

Last verified implementation commit: pending 0.2.0 release source commit

Implementation source paths: `include/elf3d`, `modules`, `facade/elf3d`,
`integrations/imgui`, `apps/viewer`, `tests`, `CMakeLists.txt`,
`CMakePresets.json`, `README.md`, `LICENSE`, `THIRD_PARTY.md`, `.github`,
`scripts`

Known limitations: Release records for 0.2.0 are tracked under
`docs/releases/0.2.0/`. Historical 0.1.0 records remain immutable under
`docs/releases/0.1.0/`.

Related documents: `docs/README.md`, `docs/releases/0.2.0/RELEASE_CHECKLIST.md`,
`docs/releases/0.2.0/VALIDATION_SUMMARY.md`

## Repository State

- Product: portable C++20 3D visualization engine
- Public library: `elf3d.dll`
- Optional integration: `elf3d_imgui`
- Reference app: `elf3d_viewer`
- Active release branch: `develop`
- Previous public release tag: `v0.1.0`
- Current release target: `v0.2.0`

## Implemented Vertical Slice

Elf3D 0.2.0 implements:

- public `Engine`, `Scene`, `Viewport`, and `SceneHierarchySnapshot` facades
- version API returning `0.2.0`
- scene entities, hierarchy, transforms, explicit local matrices, cameras,
  models, persistent visibility, bounds, hierarchy snapshots, and statistics
- scene-owned CPU mesh, image, texture, sampler, and material assets
- private static glTF/GLB importer for bounded static triangle geometry
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
  light Low.3D-inspired panels, generated PNG toolbar icons, and command toolbar

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

## Validation State

0.2.0 validation is recorded in `docs/releases/0.2.0/` and includes fresh
Debug and Release configure/build/CTest, viewer smoke, package creation,
archive inspection, extracted-package smoke, CI, GitHub Release asset
verification, and public clone testing.

Not yet validated:

- performance benchmark metrics
- external model corpus

## Known Limitations

- C++ DLL ABI requires compatible compiler/standard library/runtime.
- No stable C ABI.
- No runtime plugin ABI.
- Only OpenGL 4.1 backend.
- Opaque rendering only.
- glTF alpha factor, alpha mask, and alpha blend are not rendered.
- No animations, skins, morphs, compression extensions, KTX2, cameras, lights,
  normal maps, occlusion, or emissive material support.
- One selected entity and one distance measurement per viewport.
- Clipping boxes are axis aligned.
- Scene mutation and rendering are single-threaded.
