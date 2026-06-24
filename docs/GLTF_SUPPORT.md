# glTF Support

Purpose: Record the verified static glTF and GLB support status for Elf3D
0.1.0.

Applicable version: 0.1.0

Document status: Verified from importer code, tests, README, audit, and
Debug/Release CTest on 2026-06-23.

Last verified Git commit: `8504068`

Implementation source paths: `modules/gltf/src/importer.cpp`,
`modules/gltf/tests/gltf_importer_test.cpp`, `tests/fixtures/textured_pbr.gltf`,
`modules/image/src/image_decoder.cpp`, `modules/assets/src/storage.cpp`

Known limitations: This is not full glTF 2.0 support. Animation, skins, morph
targets, Draco, meshopt, KTX2, normal maps, occlusion, emissive, texture
transforms, additional UV sets, cameras, and lights are not imported.

Related documents: `PUBLIC_API_OVERVIEW.md`, `RENDERING_PIPELINE.md`,
`USER_GUIDE.md`, `TESTING.md`

## Summary

Elf3D 0.1.0 imports bounded static triangle geometry from `.gltf` and `.glb`
files synchronously. Successful import creates a new `Scene`. Failed import
returns a structured error and does not modify the caller's existing scene.

## Feature Matrix

| Area | Status | Behavior |
| --- | --- | --- |
| `.gltf` JSON files | Supported | Parsed through cgltf with file type checked against extension. |
| `.glb` binary files | Supported | GLB container accepted when file type matches extension. |
| External buffers | Supported | Relative local buffers accepted with size limits. |
| GLB buffers | Supported | Embedded GLB buffer data accepted. |
| Data URI buffers | Supported | Accepted through cgltf buffer loading. |
| Remote URI buffers/images | Unsupported | Rejected. |
| Default scene | Supported | Imports `scene`; falls back to first scene, then parentless nodes. |
| Node hierarchy | Supported | Reachable nodes imported with parent links and deterministic order. |
| Node names | Supported | Imported when enabled by `SceneLoadOptions`. |
| TRS transforms | Supported | Converted to exact local matrices. |
| Explicit node matrices | Supported | Preserved as matrices. |
| Mesh reuse | Supported | Mesh assets can be reused by multiple nodes. |
| Triangle primitives | Supported | Only `TRIANGLES` primitives are imported. |
| Non-triangle primitive modes | Unsupported | Return `unsupported_primitive_mode`. |
| Indexed geometry | Supported | Unsigned 8-, 16-, and 32-bit indices accepted. |
| Non-indexed geometry | Supported | Converted when vertex count is divisible by three. |
| Accessor offsets and strides | Supported | Handled by cgltf unpacking. |
| Normalized accessors | Supported | Normalized conversion handled by cgltf unpacking. |
| Sparse index accessor | Unsupported | Sparse indices are rejected. |
| `POSITION` | Required | Missing positions reject the primitive. |
| `NORMAL` | Supported | Used when present. |
| Generated normals | Supported | Generated when normals are absent and option allows it. |
| `TEXCOORD_0` | Supported | Used for supported textures. |
| Additional UV sets | Unsupported | Texture views with `texcoord != 0` are rejected. |
| Texture transforms | Unsupported | Texture views with transforms are rejected. |
| PBR metallic-roughness | Partially supported | RGB base color, metallic, roughness, base-color texture, metallic-roughness texture, and double-sided flag. |
| `baseColorFactor` alpha | Ignored | Material alpha is forced to `1.0F`; rendering is opaque. |
| Alpha `MASK` and `BLEND` | Partially supported | Imported as opaque and produce a load warning. |
| PNG/JPEG external images | Supported | Decoded to tightly packed RGBA8. |
| PNG/JPEG data URI images | Supported | Base64 data URI images accepted. |
| PNG/JPEG GLB buffer-view images | Supported | Requires supported MIME type/signature. |
| Other image formats | Unsupported | Rejected. |
| Sampler wrap/filter | Supported subset | glTF wrap/filter mapped to renderer sampler descriptions. |
| Optional extensions | Not interpreted | Optional extensions may load but do not change behavior. |
| Required unknown extensions | Unsupported | Rejected before import. |
| Cameras/lights | Unsupported | Not imported. |
| Animations/skins/morphs | Unsupported | Not imported. |
| Compression extensions | Unsupported | Required compression extensions fail. |

## Resource Limits

- Source file: 512 MiB
- Individual external buffer: 1 GiB
- Total loaded buffer data: 2 GiB
- cgltf allocation budget: 2 GiB
- Encoded image: 64 MiB
- Decoded image: 256 MiB
- Total decoded image storage per imported scene: 512 MiB
- Image dimension: 16384 pixels per axis
- Nodes: 65,536
- Meshes: 65,536
- Primitives: 262,144
- Accessors: 262,144
- Vertices: 50 million
- Indices: 150 million

## Proven by Tests

`elf3d.gltf_import` passed in Debug and Release during Goal 3 and after the
Goal 4 lifetime remediation. The test suite covers static imports, GLB, missing
normals, data URI paths, malformed input failures, unsupported extensions,
resource limits, buffer-view images, normalized texture coordinates, and alpha
mode warnings.

The project-owned visual fixture is `tests/fixtures/textured_pbr.gltf`.
