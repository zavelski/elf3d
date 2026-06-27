# Elf3D 0.4.0 Project State

Purpose: Living project-state baseline for the Elf3D 0.4.0 release source.

Applicable version: 0.4.0

Document status: Living project-state document.

Last verified implementation commit: pending 0.4.0 release source commit

Implementation source paths: `include/elf3d`, `modules`, `facade/elf3d`,
`integrations/imgui`, `apps/viewer`, `tests`, `CMakeLists.txt`,
`CMakePresets.json`, `README.md`, `LICENSE`, `THIRD_PARTY.md`, `.github`,
`scripts`

Known limitations: The local 0.4.0 release record is under
`docs/releases/0.4.0/`. The latest public release remains 0.2.0 until a
`v0.4.0` tag and GitHub Release are published and verified. Historical release
snapshots remain immutable.

Related documents: `docs/README.md`, `docs/MODULE_MAP.md`, `docs/TESTING.md`,
`docs/releases/0.4.0/RELEASE_CHECKLIST.md`,
`docs/releases/0.4.0/VALIDATION_SUMMARY.md`

## Repository State

- Product: portable C++20 3D visualization engine
- Public library: `elf3d.dll`
- Optional integration: `elf3d_imgui`
- Reference app: `elf3d_viewer`
- Release source branch: local `main`, synchronized locally with `develop`
- Previous public release tag: `v0.2.0`
- Current release target: `v0.4.0`
- Primary validated local toolchain: Visual Studio 2022 v17.14.35, MSVC
  19.44.35228.0, CMake 3.31.6-msvc6

## Implemented Vertical Slice

Elf3D 0.4.0 implements:

- public `Engine`, `Scene`, `Viewport`, and `SceneHierarchySnapshot` facades
- version API returning `0.4.0`
- one public `elf3d` DLL assembled from 18 internal engine OBJECT libraries
- real C++20 named-module interfaces and implementation units for every
  internal engine OBJECT-library boundary
- direct internal module imports with no retained import-only shim headers
- GLM-neutral exported module boundaries and project-owned public value types
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
- selection, visibility, isolation, distance measurement, section plane, and
  clipping-box tools
- Dear ImGui/GLFW reference viewer with docking, Droid Sans, generated toolbar
  icons, a Blender-like Open File dialog, and Windows GUI-subsystem startup

## Architecture Boundaries

Confirmed boundaries:

- `elf3d` does not depend on Dear ImGui or GLFW.
- Dear ImGui and GLFW are limited to `elf3d_imgui`, its third-party target, and
  `elf3d_viewer`.
- OpenGL and GLAD are isolated in `elf3d_backend_opengl` and viewer final
  presentation code.
- Viewer PNG toolbar decoding uses Windows WIC only inside `elf3d_viewer`.
- GLM and cgltf do not appear in public Elf3D headers.
- Scene does not depend on Renderer.
- Renderer consumes scene and asset data but does not own logical scene state.
- C++ named-module export is separate from DLL symbol export; public symbols
  remain controlled through `ELF3D_API`.

## Validation State

Local 0.4.0 validation is recorded in
`docs/releases/0.4.0/VALIDATION_SUMMARY.md`.

Not verified locally:

- complete manual viewer interaction and visual checklist
- GitHub branch CI, tag workflow, release assets, and public clone
- external model corpus
- performance benchmark metrics

## Known Limitations

- C++ DLL ABI requires a compatible compiler, standard library, and runtime.
- No stable C ABI or runtime plugin ABI.
- Named-module BMI/IFC artifacts are internal build outputs, not SDK artifacts.
- Only the OpenGL 4.1 backend is implemented and validated.
- Rendering is opaque-only; glTF alpha modes are not rendered.
- No animations, skins, morph targets, compression extensions, KTX2, cameras,
  lights, normal maps, occlusion, or emissive material support.
- One selected entity and one distance measurement per viewport.
- Clipping boxes are axis aligned.
- Scene mutation and rendering are single-threaded.
