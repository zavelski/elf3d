# Rendering Reference

Elf3D renders glTF scenes into an off-screen viewport texture that the host
application presents in its interface.

## Displayed Content

The renderer supports:

- triangle geometry with node transforms;
- base color, metallic, roughness, emissive, and occlusion material values;
- UV0 and UV1 texture coordinates and texture transforms;
- tangent-space normal maps on imported glTF triangle meshes;
- vertex colors;
- opaque, alpha-mask, and alpha-blend materials;
- double-sided and unlit materials;
- selection highlighting;
- distance-measurement overlays;
- section-plane and clipping-box filtering.

Standard PBR combines a directional key light with a built-in high-contrast
studio environment. Analytic rectangular softboxes and windows supply diffuse
fill plus shaped, roughness-dependent reflections, so glossy dielectrics and
metals respond visibly as the camera or environment moves. The environment is
lighting-only and is not drawn as a background.

The embedded `studio_environment_v3.ibl` resource is precomputed offline. Its
source radiance is normalized per channel to the solid-angle-weighted mean of
the former v1 profile (within one percent), retaining the established default
exposure while increasing reflection structure. Diffuse and specular
convolution use 1,024 samples; the 256-sample BRDF LUT remains byte-identical
to v1. The private 64-byte `ELF3DIBL` header identifies resource version 3,
followed by the unchanged 1,622,000-byte RGBA16F payload layout: a 32-pixel
diffuse cube, a 128-pixel specular cube with eight mip levels, and a 256 by 256
BRDF LUT.

Application startup and `Engine` construction do not read, validate, or upload
the resource. The first non-empty Standard PBR frame performs the lazy
transactional upload; empty, unlit, and non-Standard rendering do not touch it.

The default display transform uses fixed `0 EV` exposure and calibrated
exponential Standard tone mapping before one sRGB encoding step. Unlike PBR
Neutral, its shadow response does not crush dark rough-metal detail. Khronos
PBR Neutral and a diagnostic no-tone-map path remain selectable. The reference
viewer exposes directional intensity, environment intensity and rotation,
exposure, tone mapping, and the compatibility-only **Legacy ambient** value in
the **Rendering** panel. Automatic exposure and external HDR environments are
not supported.

## Transparency

Opaque and masked geometry is rendered before blended geometry. Blended
objects are ordered from back to front using their scene placement. Results are
most predictable for separated transparent surfaces.

## Visibility and Clipping

Scene visibility, viewport isolation, the section plane, and clipping boxes are
applied consistently to display, picking, measurement placement, visible
bounds, and camera fitting.

## Presentation Integration

The standard application framework owns rendering and presentation. An
explicit embedding host creates `EmbeddedRuntime`, keeps its compatible OpenGL
context current while creating, resizing, rendering, resolving, and destroying
viewport resources, and presents the non-owning view returned by
`EmbeddedRuntime::native_texture_view()`. The host must not delete or modify the
native texture.
