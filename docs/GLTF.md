# glTF Compatibility Reference

Elf3D loads static glTF 2.0 scenes from `.gltf` and `.glb` files.
Export preserves UTF-8 sidecar URIs and native Unicode output filenames.

The current importer first loads glTF into the static `elf3d_model`
CPU-side `elf3d::Document`. The engine retains that document in the runtime
Scene and derives runtime entities and document-backed handles without copying
canonical model assets. This Scene runtime adapter is the permanent engine
composition boundary, not migration or test compatibility code.

The static `elf3d_model` library can also export canonical `Document` data
with `save_document()`. `.gltf` writes a target-named binary sidecar and
PNG/JPEG/TIFF/EXR image sidecars; `.glb` embeds image streams by default. Original
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
per unique extension name. Node-matrix edits retain these owned blocks unchanged:
object identities and attachment tables are unchanged. The SDK does not interpret
or rotate spatial values inside opaque JSON. Structural/resource edits still
mark the complete preserved set stale; validation warns and export omits it with
one diagnostic. Failed/no-op edits preserve its current eligibility. Unknown
source indices into tables rebuilt by import are not generally remapped.
The importer-only bridge that attaches these blocks is private; the public
Document API exposes read-only metadata views and no raw-metadata setter.

## Normalized export formatting

Both containers use the same readable JSON layout: LF, tabs, a target width of
140 columns (four-column tab stops), compact short arrays, and wrapped long
arrays. Matrices show four consecutive groups of four column-major values.
Retained `extras` and extensions use the same nested formatting. Long strings
and keys remain intact and may exceed the width target.

Generated floating-point values use the shortest decimal representation that
reloads to the same SDK float, and negative zero becomes `0`. Raw metadata
numeric tokens retain their precision. Exact identity node transforms, empty
`extras` objects, and identity texture transforms with unchanged UV selection
and no additional metadata are omitted. Unused extension declarations are
removed with their payloads.

`ModelWriteOptions::numeric_policy` defaults to `round_trip`. The optional
`clean_rotation` policy removes coefficients within `1e-7` of an integer from
orthonormal node-matrix bases only; it preserves translation, geometry,
scaled/sheared bases, and opaque metadata numbers. Cleaning reports
`matrix_rotation_cleaned`. Cmd enables it. Export never mutates the Document.

Export currently represents the supported static subset and does not guarantee
lossless processing of arbitrary glTF. Data outside represented fields/scopes
and semantic remapping of unknown references remain capability gaps; review
import and export diagnostics. Matrix edits preserve eligible owned metadata;
structural/resource edits still invalidate it.

## Files and Resources

| Content | Supported input |
| --- | --- |
| Containers | JSON `.gltf` and binary `.glb` |
| Buffers | Local files, data URIs, and GLB buffer data |
| Images | PNG/JPEG pixels, supported RGB TIFF, and encoded EXR environments from local files, data URIs, and buffer views |
| Scene data | All scenes, authored default selection or absence, hierarchy, names, TRS, matrices, and perspective cameras |
| Geometry | Indexed or non-indexed triangles, triangle strips, and triangle fans |
| Attributes | Positions, normals, generated normals, authored/generated tangents, UV0, UV1, and vertex color |
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
- tangent-space normal textures, including scale, UV selection, and texture transform;
- double-sided and unlit materials;
- texture-coordinate selection and `KHR_texture_transform`;
- emissive strength, IOR, specular factors and strength/color textures;
- transmission, transform-aware volume absorption, and clearcoat with independent normal mapping;
- supported quantized attributes.

Core metallic-roughness materials receive built-in high-contrast studio
image-based lighting, including diffuse irradiance and roughness-dependent
specular reflections. This environment is renderer-owned rather than imported
from the glTF file. Missing or zero-length authored tangents for normal-mapped
triangle meshes are generated with MikkTSpace; incompatible UV data falls back
to geometric normals and is reported in Model Information. Invalid tangent
accessor structure and non-finite tangent data remain load errors. Authored scene
lights, shadows, external HDR environments, and skyboxes remain unsupported.

Legacy models that require `KHR_materials_pbrSpecularGlossiness` open through a
documented metallic-roughness approximation. Diffuse color and texture plus the
scalar glossiness value are retained; the specular-glossiness texture is not
represented and Model Information reports the material fallback.

## Diagnostics

A scene can open with compatibility diagnostics. Open **Model Information** to
review messages associated with nodes, geometry, materials, textures, or
external resources. A hard load error leaves the currently displayed scene
unchanged.

When reporting a model problem, prefer a small self-contained reproduction that
you are allowed to share.

## Optical materials and required-extension retention

`ModelMaterialDescription::volume` optionally owns `ModelVolumeDescription`:
nonnegative finite `thickness_factor`, an owner-scoped `thickness_texture` and
its mapping, `attenuation_color` in [0, 1], and an optional positive finite
`attenuation_distance`. Absence of the distance represents infinite attenuation
(the glTF default). The thickness texture follows the existing UV0/UV1 and
missing-UV fallback contract. Import and normalized export retain these values.
Standard rendering evaluates Beer–Lambert attenuation through a local parallel
slab using the complete instance transform; thickness remains in mesh units and
attenuation distance in world units. Required `KHR_materials_volume` is accepted
within this documented rendering approximation. Zero thickness remains a thin
surface, and an absent attenuation distance disables absorption.

`Document::required_extensions()` and its `DocumentView` counterpart borrow
read-only names owned by the Document. They have the same lifetime/invalidation
contract as metadata views. Export retains authored required declarations only
for surviving emitted payloads; normalized-away identity extensions and
quantization no longer used by the generated geometry are not declared.
Unsupported required extensions still fail import.

Opaque root extension tables and material links
remain attached to their Document owners during matrix edits, including material
copies created by the importer for texture fallbacks. This is JSON retention,
not vendor-specific lighting support or a promise to remap arbitrary references
into source node, mesh, texture, image, accessor, or material tables. Metadata
on unsupported scopes and unsupported known cgltf extensions remains subject to
import diagnostics. Empty metadata and exact no-op transforms may be omitted.

Node, mesh, and scene names decode JSON escapes once at import, including UTF-16
surrogate pairs, and export as valid UTF-8 JSON. Invalid escaped Unicode and
embedded nulls in these names or image URI/MIME fields fail import.

The current cgltf bridge treats its `FLT_MAX` attenuation-distance sentinel as
infinity; an authored distance at that exact binary32 boundary is interpreted
as absent. This remains an import fidelity limitation.

## Camera and image preservation

Perspective camera names, optional authored aspect ratios, and absent/infinite
far planes are represented by `ModelCameraPropertiesDescription` and exported
with their original projection semantics. The runtime perspective description
still supplies a finite far plane for rendering. Camera and perspective-object
`extras`, and camera unknown extensions, belong to their attached Document nodes;
shared source cameras can be emitted as equivalent per-node camera records.
`NodeView::camera_properties` borrows its name and metadata from the Document;
`set_node_camera_properties` validates image ownership and finite values.

Material names are retained, including names of imported fallback copies. CMYK
and YCCK JPEG textures decode into RGBA8 using Adobe-marker inversion where
present; their original JPEG streams remain eligible for byte-preserving export.
This conversion does not add ICC color management. When some authored normals
are unusable, only those vectors are repaired; valid authored directions remain.
Opaque spatial metadata is not automatically transformed or arbitrary indices
remapped.

TIFF image streams are also retained as `ModelImageMimeType::tiff`. The image
boundary decodes single-directory, uncompressed, top-left, interleaved unsigned
RGB TIFF with uniform 8-bit or 16-bit channels, in either byte order and with
validated strip offsets and sizes. Existing RGBA8 storage uses the high byte of
16-bit samples; the original TIFF stream, including all sample precision and
embedded tags, is exported byte-for-byte while pixels remain unchanged. Changed
pixels fall back to PNG. TIFF compression, tiled layouts, other photometric
interpretations, non-default orientation, and multiple directories are rejected
with a diagnostic; this is not general TIFF support or ICC color management.
TIFF and EXR MIME types are corpus-specific additions beyond core glTF's PNG/JPEG
image formats, and consumers must support them to display those resources.
The TIFF layout follows [Adobe's TIFF specifications](https://www.adobe.com/support/downloads/dng/dng_sdk.html).
