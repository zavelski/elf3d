# glTF Compatibility Reference

Elf3D loads static glTF 2.0 scenes from `.gltf` and `.glb` files.

The current importer first loads glTF into the static `elf3d_model`
CPU-side `elf3d::Document`. The engine retains that document in the runtime
Scene and derives runtime entities and document-backed handles without copying
canonical model assets. This Scene runtime adapter is the permanent engine
composition boundary, not migration or test compatibility code.

The static `elf3d_model` library can also export canonical `Document` data
with `save_document()`. `.gltf` writes a target-named binary sidecar and
PNG/JPEG image sidecars; `.glb` embeds image streams by default. Original
PNG/JPEG bytes and MIME are reused when they still decode exactly to the
current pixels, otherwise export falls back to PNG and records the re-encoding
in `ModelWriteReport`. The complete output set is staged before replacement.
If replacement fails, pre-existing primary and sidecar files are restored; if
restoration itself fails, the recovery backup is retained and reported.
The exporter does not use runtime Scene, renderer, GPU, or viewer state.
Import bounds each encoded image to 64 MiB and the total retained encoded/source
image data to 512 MiB per document. Total decoded RGBA8 image data is bounded
to 2 GiB in 64-bit builds and 512 MiB in 32-bit builds.
Source files and embedded GLB BIN chunks are bounded to 3 GiB; individual
external buffers remain bounded to 1 GiB. Signed 32-bit size/offset overflow
from affected GLB exporters is repaired only for an unambiguous sequential,
4-byte-aligned BIN layout and is reported as a compatibility diagnostic.

When a texture has no sampler or its sampler omits `minFilter`, Elf3D uses
linear filtering with a complete trilinear mip chain down to 1x1. Explicit
glTF minification and magnification filters are preserved.

All glTF scenes are retained and exported in source order. The authored default
scene is preserved, including the absence of a top-level `scene` selection.
For engine loading only, the first scene is the effective runtime selection
when no default was authored.
Imported scene hierarchy depth is limited to 8,192.
An empty scene is exported without a `nodes` member. A Document mesh with no
primitives is rejected with `invalid_mesh_data` before any output is staged or
published.

Raw `extras` and unknown extension JSON are preserved at root, asset, scene,
node, mesh, primitive, material, image, texture, and sampler scopes within
bounded limits: 1 MiB per JSON value, 64 MiB total per document, and 256 bytes
per unique extension name. Because unknown JSON references cannot be remapped
safely, any successful Document mutation marks the complete preserved set
stale; validation warns and export omits it with one diagnostic.
The importer-only bridge that attaches these blocks is private; the public
Document API exposes read-only metadata views and no raw-metadata setter.

## Files and Resources

| Content | Supported input |
| --- | --- |
| Containers | JSON `.gltf` and binary `.glb` |
| Buffers | Local files, data URIs, and GLB buffer data |
| Images | PNG and JPEG from local files, data URIs, and buffer views |
| Scene data | All scenes, authored default selection or absence, hierarchy, names, TRS, matrices, and perspective cameras |
| Geometry | Indexed or non-indexed triangles, triangle strips, and triangle fans |
| Attributes | Positions, normals, generated normals, UV0, UV1, and vertex color |
| Sparse data | Sparse vertex attributes |

Keep external buffers and images at the paths referenced by the `.gltf` file.
For portable sharing, use a self-contained `.glb` or copy the complete asset
directory.

## Materials

The viewer displays:

- base color factors, textures, and vertex color;
- metallic and roughness factors and textures;
- opaque, alpha-mask, and alpha-blend materials;
- emissive and occlusion values and textures;
- double-sided and unlit materials;
- texture-coordinate selection and `KHR_texture_transform`;
- emissive strength, IOR, specular factors, and supported quantized attributes.

## Diagnostics

A scene can open with compatibility diagnostics. Open **Model Information** to
review messages associated with nodes, geometry, materials, textures, or
external resources. A hard load error leaves the currently displayed scene
unchanged.

When reporting a model problem, prefer a small self-contained reproduction that
you are allowed to share.
