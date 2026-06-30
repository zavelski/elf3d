# Elf3D 0.5.0 Project State

Purpose: Living project-state baseline for the glTF compatibility development
milestone after the audited 0.4.0 source.

Applicable version: 0.5.0

Document status: Living development-state document. Versioned 0.4.0 release
records under `docs/releases/0.4.0/` remain immutable.

Baseline implementation commit: `e974ff9ddf1bee8bf3ae4f0e645b3840280e3943`

Implementation source paths: `include/elf3d`, `modules`, `facade/elf3d`,
`integrations/imgui`, `apps/viewer`, `tests`, `CMakeLists.txt`,
`CMakePresets.json`, `README.md`, `.github`, `scripts`

Related documents: `docs/GLTF_SUPPORT.md`, `docs/RENDERING_PIPELINE.md`,
`docs/PUBLIC_API_OVERVIEW.md`, `docs/TESTING.md`, `docs/ROADMAP.md`

## Repository State

- Product: portable C++20 3D visualization engine
- Public library: `elf3d.dll`
- Optional integration: `elf3d_imgui`
- Reference app: `elf3d_viewer`
- Internal composition: 18 CMake OBJECT libraries with named C++20 modules
- Primary validated toolchain: Visual Studio 2022/MSVC on Windows x64
- Graphics backend: OpenGL 4.1 core

## Implemented Vertical Slice

Elf3D 0.5.0 includes the 0.4.0 visualization/tool baseline plus:

- fixed bounded UV0/UV1 vertex storage;
- independent material texture `texCoord` selection;
- full supported-slot `KHR_texture_transform` offset, scale, rotation, and UV
  override;
- vertex `COLOR_0`;
- base-color alpha, alpha mask/cutoff, and simple sorted alpha blending;
- emissive factor/texture and emissive strength;
- occlusion texture/strength;
- unlit materials;
- IOR and specular factor/color shading;
- normal texture/scale/mapping preservation with a structured render fallback;
- triangle-strip and triangle-fan conversion;
- perspective-camera import;
- structured public load diagnostics through `load_scene_with_report`;
- viewer display of successful-load diagnostics;
- a repeatable public-API private-corpus probe.

## Architecture Boundaries

The change preserves the existing dependency direction:

- cgltf remains private to `elf3d_gltf`;
- OpenGL/GLAD remain private to `elf3d_backend_opengl`;
- Scene and Assets remain format-neutral;
- Scene does not depend on Renderer;
- Renderer consumes Scene/Asset values without mutating them;
- the viewer uses only the public `elf3d` API and optional ImGui integration;
- no third-party type was added to a public header;
- no dependency revision changed.

The public API addition is source-compatible at the entry-point level:
`Engine::load_scene` remains available, while `Engine::load_scene_with_report`
returns a `LoadedScene` and `SceneLoadReport`. Public vertex/material value
layouts changed, so matched-toolchain DLL consumers must rebuild for 0.5.0.

## Compatibility Behavior

Fully rendered extension paths:

- `KHR_texture_transform`
- `KHR_materials_unlit`
- `KHR_materials_emissive_strength`
- `KHR_materials_ior`
- `KHR_mesh_quantization` for imported supported attributes

`KHR_materials_specular` factors/color render; its textures use a diagnostic
fallback. Optional advanced material, compression, KTX2/BasisU, light,
variant, and GPU-instancing extensions load core/fallback data where possible
and report diagnostics. Unsupported required extensions fail clearly.

## Validation State

Fresh Windows Debug and Release configurations build all targets under the
warning-as-error policy, and both configurations pass all 17 default CTest
tests. The conditional local-corpus CTest also passes against the project
fixture. Manual Release-viewer validation covered the procedural scene, the
project-owned textured fixture, and the generated UV1/texture-transform
fixture; rendering, scene statistics, and load diagnostics were visible.

No user-provided real-file corpus was attached or found in the workspace. The
project-owned `tests/fixtures/textured_pbr.gltf` probe passes without hard
errors or diagnostics; this does not replace the pending user corpus.

## Known Limitations

- UV sets are intentionally bounded to `TEXCOORD_0` and `TEXCOORD_1`.
- Normal textures are preserved but not rendered until tangent import/
  generation and handedness are implemented.
- Blend sorting is per model origin, not per triangle or order-independent.
- Picking does not sample alpha-masked or blended texture alpha.
- No animation playback, skinning, morph deformation, scene-light model,
  orthographic camera model, Draco/meshopt decoder, or KTX2/BasisU decoder.
- No clearcoat, sheen, transmission, volume, or material-variant render model.
- Loading remains synchronous and scene mutation/rendering remain
  single-threaded.
- The public API remains a matched-toolchain C++ ABI, not a stable C ABI.
