# Elf3D 0.1.0 Project State

Purpose: Verified project-state baseline for the current 0.1.0 audit branch.

Applicable version: 0.1.0

Document status: Living project-state document created because the requested
`PROJECT_STATE_EN.md` was absent during Goal 1.

Last verified Git commit: `8504068`

Implementation source paths: `include/elf3d`, `modules`, `facade/elf3d`,
`integrations/imgui`, `apps/viewer`, `tests`, `CMakeLists.txt`,
`CMakePresets.json`, `README.md`, `THIRD_PARTY.md`

Known limitations: This is not the immutable release snapshot. The release
snapshot is created under `docs/releases/0.1.0/` during release preparation.

Related documents: `docs/README.md`, `docs/audits/ELF3D_0.1.0_AUDIT.md`,
`docs/audits/ELF3D_0.1.0_VALIDATION_MATRIX.md`

## Repository State

- Product: portable C++20 3D visualization engine
- Public library: `elf3d.dll`
- Optional integration: `elf3d_imgui`
- Reference app: `elf3d_viewer`
- Current audit branch: `audit/0.1.0`
- Pre-audit checkpoint: `f8fe3a827bc81dadb461e58bdbe846958dab346a`
- Latest remediation log commit before this document: `8504068`
- Remotes: none configured during audit
- Tags: none present during audit

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

## Architecture Boundaries

Confirmed boundaries:

- `elf3d` does not depend on Dear ImGui or GLFW.
- Dear ImGui and GLFW are limited to `elf3d_imgui`, its third-party target, and
  `elf3d_viewer`.
- OpenGL and GLAD are isolated in `elf3d_backend_opengl` and viewer final
  presentation code.
- GLM and cgltf do not appear in public Elf3D headers.
- Scene does not depend on renderer.
- Renderer consumes scene and asset data but does not own logical scene state.

## Validation State

Completed validation:

- Debug configure/build passed.
- Debug CTest passed 16 of 16.
- Release configure/build passed.
- Release CTest passed 16 of 16.
- Debug viewer process started with `tests/fixtures/textured_pbr.gltf` and
  remained alive for five seconds.
- Release viewer process started with `tests/fixtures/textured_pbr.gltf` and
  remained alive for five seconds.

Not yet validated:

- manual visual rendering correctness
- manual navigation, picking, selection, measurement, clipping interaction
- normal user-driven viewer shutdown
- performance benchmark metrics
- external model corpus
- CI

## Remediated Audit Items

- AUD-002: CMake/CTest not on shell `PATH` was resolved by using Visual
  Studio's bundled CMake/CTest.
- AUD-003: Scene cache release no longer stores a raw `Engine::Impl*`; commit
  `7957aee` introduced a private weak release context.

## Remaining Release Work

- Complete release documentation and release snapshot.
- Decide whether import warnings remain `std::clog` diagnostics for 0.1.0 or
  need a public report API before release.
- Manually validate the viewer.
- Prepare `CHANGELOG.md` and release checklist.
- Run final clean validation.
- Make the Goal 7 release decision.
- Create `v0.1.0` only if no release blockers remain.

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
