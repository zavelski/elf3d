# Elf3D 0.1.0 Project State

Purpose: Verified project-state baseline for the current 0.1.0 publication
preparation branch.

Applicable version: 0.1.0

Document status: Living project-state document.

Last verified implementation commit before final package-record update:
`a99bb10`

Implementation source paths: `include/elf3d`, `modules`, `facade/elf3d`,
`integrations/imgui`, `apps/viewer`, `tests`, `CMakeLists.txt`,
`CMakePresets.json`, `README.md`, `LICENSE`, `THIRD_PARTY.md`, `.github`,
`scripts`

Known limitations: Release records exist under `docs/releases/0.1.0/`, and the
release was publicly published after user-performed packaged viewer interaction
validation, final local validation, branch integration, corrected branch CI,
tag-triggered release workflow verification, GitHub Release asset verification,
and public clone validation passed.

Related documents: `docs/README.md`, `docs/audits/ELF3D_0.1.0_AUDIT.md`,
`docs/audits/ELF3D_0.1.0_VALIDATION_MATRIX.md`,
`docs/releases/0.1.0/PROJECT_STATE_EN.md`,
`docs/releases/0.1.0/RELEASE_CHECKLIST.md`

## Repository State

- Product: portable C++20 3D visualization engine
- Public library: `elf3d.dll`
- Optional integration: `elf3d_imgui`
- Reference app: `elf3d_viewer`
- Current preparation branch: `audit/0.1.0`
- Pre-audit checkpoint: `f8fe3a827bc81dadb461e58bdbe846958dab346a`
- Latest local publication-prep commit before final validation-record update:
  `f4d7d8e`
- Remotes: none configured during publication precheck
- Tags: none present during publication precheck
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

- Debug configure/build passed in a fresh `windows-debug` tree.
- Debug CTest passed 16 of 16.
- Release configure/build passed in a separate `windows-release` tree.
- Release CTest passed 16 of 16.
- Debug viewer opened `tests/fixtures/textured_pbr.gltf` and exited with code 0
  after `CloseMainWindow()`.
- Release viewer opened `tests/fixtures/textured_pbr.gltf`, rendered the
  fixture in a captured screenshot, and exited with code 0 after
  `CloseMainWindow()`.
- `elf3d-viewer-0.1.0-windows-x64.zip` and `SHA256SUMS.txt` were regenerated
  and inspected after the final Release build; the final viewer ZIP SHA-256 is
  `1d39c50460e86083f448557ed6a7eddad3974d26b99e84e4c2cfc030c5265c92`.
- The final packaged viewer opened from the extracted package directory and
  exited with code 0 after `CloseMainWindow()`.
- User-performed manual validation on the packaged Windows Release viewer
  passed for navigation, picking, selection, hierarchy synchronization,
  visibility, isolation, measurement, clipping, reload, close-scene,
  failed-load preservation, and normal shutdown.
- Tag-triggered GitHub Release workflow passed on `windows-2022`, created the
  public `v0.1.0` GitHub Release, and uploaded verified release assets.
- Fresh public clone of `https://github.com/zavelski/elf3d.git` checked out
  `v0.1.0`, configured, built, and passed Debug and Release CTest 16/16.
- Public headers compiled individually as forced includes with MSVC C++20,
  `/permissive-`, `/W4`, and `/WX` during earlier audit validation.
- Publication-prep release records were updated under `docs/releases/0.1.0/`.

Not yet validated:

- performance benchmark metrics
- external model corpus

## Remediated Audit Items

- AUD-002: CMake/CTest not on shell `PATH` was resolved by using Visual
  Studio's bundled CMake/CTest.
- AUD-003: Scene cache release no longer stores a raw `Engine::Impl*`; commit
  `7957aee` introduced a private weak release context.

## Remaining Release Work

- Decide whether import warnings remain `std::clog` diagnostics for 0.1.x or
  need a public report API.
- Keep the published `v0.1.0` tag unchanged.
- Use post-publication documentation on `develop` for additional release
  records.

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
