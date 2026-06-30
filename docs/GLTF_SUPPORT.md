# glTF Support

Purpose: Record the verified static glTF and GLB support status for Elf3D
0.7.2.

Applicable version: 0.7.2

Document status: Living compatibility matrix verified from importer, renderer,
public API, viewer, and tests.

Implementation source paths: `modules/gltf`, `modules/assets`, `modules/scene`,
`modules/renderer`, `modules/backend_opengl`, `facade/elf3d`,
`tests/gltf_corpus_probe.cpp`

Known limitations: This is not full glTF 2.0 support. Animation, skinning,
morph deformation, scene lights, orthographic cameras, tangent-space normal
mapping, compression decoders, KTX2/BasisU, material variants, and advanced
layered/transmissive materials remain outside the implemented render path.

Related documents: `PUBLIC_API_OVERVIEW.md`, `RENDERING_PIPELINE.md`,
`USER_GUIDE.md`, `TESTING.md`

## Summary

Elf3D 0.7.2 imports bounded static geometry from `.gltf` and `.glb`
synchronously. UV sets 0 and 1, per-texture UV selection, and
`KHR_texture_transform` are preserved through the asset model and renderer.
Successful imports may return structured diagnostics through
`Engine::load_scene_with_report`; failed imports return a structured `Error` and
do not replace an existing caller-owned scene.

## Core Feature Matrix

| Area | Status | Behavior |
| --- | --- | --- |
| `.gltf` / `.glb` | Supported | JSON glTF and GLB 2.0 containers with local, data-URI, or GLB buffers. |
| Node hierarchy and transforms | Supported | Selected-scene hierarchy, names, TRS, and matrices are preserved. |
| Mesh reuse | Supported | Imported mesh assets may be referenced by several nodes. |
| `TRIANGLES` | Supported | Indexed and non-indexed triangle lists. |
| `TRIANGLE_STRIP` / `TRIANGLE_FAN` | Supported | Converted to triangle-list indices with strip winding preserved. |
| Points and line modes | Unsupported | Import fails with `unsupported_primitive_mode`. |
| `POSITION` / `NORMAL` | Supported | Normals are normalized; optional deterministic generation is available. |
| `TEXCOORD_0` / `TEXCOORD_1` | Supported | Both fixed UV sets are stored per vertex and selectable independently per texture slot. |
| `TEXCOORD_n`, `n >= 2` | Bounded fallback | Unreferenced sets are ignored with a diagnostic; a referenced set above 1 fails clearly. |
| Missing referenced UV set | Rejected | A texture that selects an absent UV set fails with `invalid_texcoord`; UV0 is never silently substituted for UV1. |
| `COLOR_0` | Supported | VEC3/VEC4, including normalized integer accessors, multiplies base color. |
| `TANGENT` | Not rendered | Tangents are not preserved because normal mapping is not yet rendered. |
| Sparse vertex accessors | Supported | cgltf float unpacking applies sparse values for supported attributes. |
| Sparse index accessors | Unsupported | Rejected because cgltf does not unpack sparse index accessors. |
| PNG / JPEG | Supported | External, data-URI, and buffer-view images decode to bounded RGBA8. |
| Other core image formats | Unsupported | Clear MIME/extension/decode errors are returned. |
| Perspective cameras | Supported | Imported on their authored node; infinite far planes use a documented 1e9 fallback. |
| Orthographic cameras | Diagnostic fallback | Node remains transform-only and the load report explains the missing camera model. |
| Animations | Diagnostic fallback | Static authored node transforms load; animation clips/channels are ignored. |
| Skins | Diagnostic fallback | Undeformed mesh geometry loads with a warning. |
| Morph targets | Diagnostic fallback | Base mesh geometry loads with a warning. |

The vertex layout intentionally has a fixed limit of two UV sets. This avoids
an unbounded per-vertex representation while covering the practical
`TEXCOORD_1` blocker. Raising the limit later requires an explicit asset and GPU
layout change.

## Material Feature Matrix

| Area | Status | Behavior |
| --- | --- | --- |
| Base-color factor RGBA | Supported | RGB and alpha are preserved. |
| Base-color texture | Supported | sRGB sampling, UV0/UV1 selection, independent transform. |
| Metallic/roughness factors and texture | Supported | Linear sampling, UV0/UV1 selection, independent transform. |
| `OPAQUE` | Supported | Base alpha is ignored and output remains opaque. |
| `MASK` / `alphaCutoff` | Supported | Fragments below cutoff are discarded. |
| `BLEND` | Supported, simple policy | Output-space source-over blending, depth writes disabled, blended model primitives sorted back-to-front by model origin. Intersecting transparent geometry is not order-independent. |
| Emissive factor / texture | Supported | Emissive RGB is added in linear space; texture has independent UV selection/transform. |
| Occlusion texture / strength | Supported | Red channel attenuates ambient contribution; independent UV selection/transform. |
| Normal texture / scale | Preserved fallback | Texture, scale, UV selection, and transform are imported, but rendering emits a diagnostic and uses vertex normals until tangents/tangent generation are implemented. |
| Double-sided | Supported | Culling and back-face normal handling are retained. |
| Unlit | Supported | Base color/texture/vertex color render without the lighting BRDF. |
| IOR | Supported | Dielectric F0 derives from the imported index of refraction. |
| Specular factor/color | Supported | Factors participate in dielectric Fresnel. Specular textures are ignored with a diagnostic. |

Alpha-masked transparent texels are discarded in the visible render pass.
Picking remains geometry-based and does not sample material alpha.

## Extension Matrix

| Extension | Classification | Behavior |
| --- | --- | --- |
| `KHR_texture_transform` | Fully implemented and rendered | Offset, scale, counter-clockwise rotation, and extension `texCoord` override apply independently per supported core texture slot. |
| `KHR_materials_unlit` | Fully implemented and rendered | Uses the unlit base-color path. |
| `KHR_materials_emissive_strength` | Fully implemented and rendered | Multiplies the emissive factor before rendering. |
| `KHR_materials_ior` | Fully implemented and rendered | Changes dielectric Fresnel. |
| `KHR_materials_specular` | Partially implemented | Factor/color render; optional textures are ignored with a material diagnostic. Required usage is accepted only when those textures are absent. |
| `KHR_mesh_quantization` | Implemented for imported attributes | Supported component types are converted through cgltf accessor unpacking. |
| `KHR_lights_punctual` | Parsed, diagnostic fallback | Optional lights do not block geometry loading; no scene-light model exists yet. Required use fails. |
| `KHR_materials_clearcoat` | Core-material fallback | Optional usage loads with a warning; required usage fails. |
| `KHR_materials_sheen` | Core-material fallback | Optional usage loads with a warning; required usage fails. |
| `KHR_materials_transmission` | Core-material fallback | Optional usage loads with a warning; required usage fails. |
| `KHR_materials_volume` | Core-material fallback | Optional usage loads with a warning; required usage fails. |
| `KHR_materials_pbrSpecularGlossiness` | Approximate fallback | Without a core material, diffuse factor/texture and glossiness-to-roughness are approximated; the specular-glossiness texture is ignored with diagnostics. Required usage fails. |
| `KHR_materials_variants` | Default-material fallback | Primitive default material is used with a warning; required usage fails. |
| `KHR_draco_mesh_compression` | No decoder | Optional usage can load ordinary fallback geometry; required usage fails. |
| `EXT_meshopt_compression` | No decoder | Optional usage can load ordinary fallback buffer data; required usage fails. |
| `KHR_texture_basisu` | No decoder | Optional usage uses ordinary PNG/JPEG fallback or disables the slot with a warning; required usage fails. |
| `EXT_texture_webp` | No decoder | Optional usage uses an ordinary PNG/JPEG fallback or disables the slot with a warning; required usage fails. |
| `EXT_mesh_gpu_instancing` | No expansion | Base node loads once with a warning; required usage fails. |
| Unknown optional extension | Ignored with diagnostic | Core/fallback data is loaded where possible. |
| Unknown required extension | Rejected | `unsupported_required_extension` identifies the extension. |

Clearcoat, sheen, transmission, volume, Draco, meshopt, and KTX2/BasisU need
larger renderer or third-party dependency decisions. They are not presented as
full-fidelity support.

## Resource Limits

- Source file: 512 MiB
- Individual external buffer: 1 GiB
- Total loaded buffer data: 2 GiB
- cgltf allocation budget: 2 GiB
- Encoded image: 64 MiB
- Decoded image: 256 MiB
- Total decoded image storage per scene: 512 MiB
- Image dimension: 16384 pixels per axis
- Nodes: 65,536
- Meshes: 65,536
- Primitives: 262,144
- Accessors: 262,144
- Vertices: 50 million
- Imported triangle-list indices: 150 million after non-indexed primitives and
  `TRIANGLE_STRIP` / `TRIANGLE_FAN` expansion are accounted for.

## Validation Coverage

`elf3d.gltf_import` covers UV0/UV1 preservation, explicit `texCoord: 1`,
missing referenced UV sets, all `KHR_texture_transform` fields, vertex color,
alpha factors/modes/cutoff, emissive/normal/occlusion slots, unlit, emissive
strength, IOR, specular factors, supported and unsupported required extensions,
optional extension diagnostics, perspective cameras, and indexed/non-indexed
strip/fan conversion, including an oversized strip that must fail before buffer
loading because its expanded triangle-list index count exceeds the importer
limit. Renderer tests validate that mappings and material values reach the
graphics boundary and that the shader contains the corresponding paths.

The repeatable private-corpus workflow is documented in `TESTING.md`. No
user-provided real-file corpus was present in the workspace for this milestone
run; the project-owned `tests/fixtures/textured_pbr.gltf` probe passed without
hard errors or diagnostics.
