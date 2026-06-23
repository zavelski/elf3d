# Elf3D 0.1.0 Release Snapshot Project State

Purpose: Immutable release-candidate snapshot of the verified Elf3D 0.1.0
project state.

Applicable version: 0.1.0

Document status: Release snapshot, not a living document.

Last verified implementation commit before release snapshot: `79fd4bc`

Implementation source paths: `include/elf3d`, `modules`, `facade/elf3d`,
`integrations/imgui`, `apps/viewer`, `tests`, `CMakeLists.txt`,
`CMakePresets.json`, `README.md`, `THIRD_PARTY.md`

Known limitations: The release is not tagged because manual visual viewer
validation has not been performed.

Related documents: `AUDIT_SUMMARY.md`, `VALIDATION_SUMMARY.md`,
`KNOWN_LIMITATIONS.md`, `RELEASE_CHECKLIST.md`, `../../../PROJECT_STATE_EN.md`

## Repository State

- Product: portable C++20 3D visualization engine
- Public library: `elf3d.dll`
- Optional integration: `elf3d_imgui`
- Reference app: `elf3d_viewer`
- Release-candidate branch: `audit/0.1.0`
- Pre-audit checkpoint: `f8fe3a827bc81dadb461e58bdbe846958dab346a`
- Remotes during audit: none configured
- Tags during audit: none present
- Release decision: Not ready due to release blockers

## Implemented Vertical Slice

Elf3D 0.1.0 implements:

- public `Engine`, `Scene`, `Viewport`, and `SceneHierarchySnapshot` facades
- version API returning `0.1.0`
- scene entities, hierarchy, transforms, explicit local matrices, cameras,
  models, persistent visibility, bounds, hierarchy snapshots, and statistics
- scene-owned CPU mesh, image, texture, sampler, and material assets
- private static glTF/GLB importer for bounded static triangle geometry
- private PNG/JPEG decode to RGBA8
- OpenGL 4.1 off-screen viewport rendering
- opaque metallic-roughness directional-light shader path
- GPU mesh and texture caches
- CPU picking with per-mesh BVH cache
- viewport orbit/pan/wheel navigation, fit, and reset
- single selection per viewport
- scene visibility and viewport isolation
- one distance measurement per viewport
- one section plane and up to three axis-aligned clipping boxes per viewport
- Dear ImGui/GLFW reference viewer

## Confirmed Boundaries

- `elf3d` does not depend on Dear ImGui or GLFW.
- Dear ImGui and GLFW are limited to `elf3d_imgui`, its third-party target, and
  `elf3d_viewer`.
- OpenGL and GLAD are isolated in `elf3d_backend_opengl` and viewer final
  presentation code.
- GLM and cgltf do not appear in public Elf3D headers.
- Scene does not depend on renderer.
- Renderer consumes scene and asset data but does not own logical scene state.

## Validation State

Completed on 2026-06-23:

- Debug configure/build passed.
- Debug CTest passed 16 of 16.
- Release configure/build passed.
- Release CTest passed 16 of 16.
- Public header self-containment check passed for all public headers under
  `include/elf3d`.
- Debug viewer process started with `tests/fixtures/textured_pbr.gltf` and
  remained alive for five seconds.
- Release viewer process started with `tests/fixtures/textured_pbr.gltf` and
  remained alive for five seconds.

Not validated:

- manual visual rendering correctness
- manual navigation, picking, selection, measurement, clipping interaction
- normal user-driven viewer shutdown
- performance benchmark metrics
- external model corpus
- CI

## Release Decision

`Not ready due to release blockers`

Remaining blocker:

- Manual visual viewer validation has not been performed. The viewer process
  smoke test does not prove rendering correctness, interaction behavior, or
  normal user-driven shutdown.
