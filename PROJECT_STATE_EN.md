# Elf3D 0.7.2 Project State

Purpose: Living project-state baseline for the glTF compatibility development
milestone after the audited 0.4.0 source.

Applicable version: 0.7.2

Document status: Living release-state document for the local 0.7.2 source.
Versioned 0.4.0 release records under `docs/releases/0.4.0/` remain immutable.

Last verified Git commit: local tag `v0.7.2` after release commit

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

Elf3D 0.7.2 carries forward the published 0.7.1 static glTF/GLB compatibility,
viewport UX, renderer color-space, and release baseline. It adds focused
alpha-channel correctness, OpenGL resource cleanup, release workflow, and
package reproducibility fixes from the post-0.7.1 audit.

The glTF/static-visualization baseline includes:

- fixed bounded UV0/UV1 vertex storage;
- independent material texture `texCoord` selection;
- full supported-slot `KHR_texture_transform` offset, scale, rotation, and UV
  override;
- vertex `COLOR_0`;
- base-color alpha, alpha mask/cutoff, and simple sorted linear alpha blending;
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

Carried-forward 0.7.1 stabilization work includes:

- About dialog first-open centering using Dear ImGui next-window positioning;
- hover-based mouse-wheel dolly when the cursor is inside the 3D view, without
  requiring a click to restore 3D-view focus;
- wheel zoom stability after a quick click updates a dynamic examine pivot;
- focused navigation regression tests for the hover-wheel and click-pivot wheel
  paths;
- linear off-screen color composition with a display resolve pass that applies
  sRGB transfer encoding after blending;
- a real hidden-context OpenGL smoke test for GLSL compilation and transparent
  pixel output;
- glTF strip/fan resource-limit validation against the expanded imported
  triangle-list index count;
- host-owned scene-load diagnostics with no `std::clog` output from
  `Engine::load_scene`;
- refreshed 0.7.2 version metadata, packaging metadata, workflow metadata,
  living documentation, and local release records.

New 0.7.2 audit fixes include:

- separate OpenGL alpha blending factors for material and overlay draws so the
  viewport texture alpha remains correct over an opaque clear;
- hidden-context OpenGL smoke coverage for resolved texture extent and alpha;
- immediate display-resolve shader/VAO release when a viewport render target is
  resized to zero;
- default package-version derivation from `CMakeLists.txt`;
- sorted deterministic ZIP packaging with fixed entry timestamps for fixed
  staged file sets;
- release workflow version derivation from CMake with tag validation and
  version reuse for assets, title, and release notes.

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

The carried-forward public glTF diagnostics API is source-compatible at the
entry-point level: `Engine::load_scene` remains available, while
`Engine::load_scene_with_report` returns a `LoadedScene` and `SceneLoadReport`.
Public vertex/material value layouts include the glTF compatibility baseline,
so matched-toolchain DLL consumers must rebuild for 0.7.2.

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

Fresh Windows Debug and Release configure/build/test validation passed for the
0.7.2 local release source. The Release glTF probe passed against
`tests/fixtures/textured_pbr.gltf`, the 0.7.2 package was generated and
inspected, and the extracted packaged viewer process stayed alive for a
short process-smoke run.

The automated test set now includes `elf3d.opengl_render_smoke`, which passed
locally with a hidden real OpenGL context and verified GLSL compilation plus a
linear transparent-blend center pixel.

No visible/manual viewer interaction pass was rerun after the 0.7.2 renderer
and importer fixes, so the local release does not claim visual inspection of the
packaged viewer beyond the hidden-context OpenGL smoke and process launch.

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
