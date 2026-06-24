# Elf3D 0.1.0 Release Snapshot Project State

Purpose: Publication-prep snapshot of the verified Elf3D 0.1.0 project state.

Applicable version: 0.1.0

Document status: Publication-prep release snapshot.

Last verified implementation commit before final package-record update:
`a99bb10`

Implementation source paths: `include/elf3d`, `modules`, `facade/elf3d`,
`integrations/imgui`, `apps/viewer`, `tests`, `CMakeLists.txt`,
`CMakePresets.json`, `README.md`, `LICENSE`, `THIRD_PARTY.md`, `.github`,
`scripts`

Known limitations: The release is ready for public tag publication after
user-performed packaged viewer interaction validation, final local validation,
branch integration, and successful corrected branch CI. Tag-triggered release
workflow verification, GitHub Release verification, and public clone validation
still need to run.

Related documents: `AUDIT_SUMMARY.md`, `VALIDATION_SUMMARY.md`,
`KNOWN_LIMITATIONS.md`, `RELEASE_CHECKLIST.md`, `../../../PROJECT_STATE_EN.md`

## Repository State

- Product: portable C++20 3D visualization engine
- Public library: `elf3d.dll`
- Optional integration: `elf3d_imgui`
- Reference app: `elf3d_viewer`
- Release-preparation branch: `audit/0.1.0`
- Pre-audit checkpoint: `f8fe3a827bc81dadb461e58bdbe846958dab346a`
- Remotes during publication precheck: none configured
- Tags during publication precheck: none present
- Release decision: `GO — ready for public publication`

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
  `include/elf3d` during earlier audit validation.
- Debug and Release viewer processes opened `tests/fixtures/textured_pbr.gltf`
  and exited cleanly through the window close path.
- Release viewer screenshot showed the project-owned fixture rendered.
- Windows viewer ZIP package and checksum were regenerated and inspected after
  the final Release build; the final viewer ZIP SHA-256 is
  `1d39c50460e86083f448557ed6a7eddad3974d26b99e84e4c2cfc030c5265c92`.
- Extracted final packaged viewer started from the extracted package directory
  and exited cleanly after `CloseMainWindow()`.
- User-performed manual validation on the packaged Windows Release viewer
  passed for navigation, picking, selection, hierarchy synchronization,
  visibility, isolation, measurement, clipping, reload, close-scene,
  failed-load preservation, and normal shutdown.

Not validated:

- performance measurements
- external model corpus
- public clone test

## Release Decision

`GO — ready for public publication`

Remaining publication validation:

- Annotated tag publication, tag-triggered release workflow verification,
  GitHub Release verification, and public clone test.
