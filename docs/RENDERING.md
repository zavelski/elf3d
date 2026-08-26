# Rendering Reference

Elf3D renders glTF scenes into an off-screen viewport texture that the host
application presents in its interface.

## Displayed Content

The renderer supports:

- triangle geometry with node transforms;
- base color, metallic, roughness, emissive, and occlusion material values;
- UV0 and UV1 texture coordinates and texture transforms;
- vertex colors;
- opaque, alpha-mask, and alpha-blend materials;
- double-sided and unlit materials;
- selection highlighting;
- distance-measurement overlays;
- section-plane and clipping-box filtering.

Standard PBR combines a directional key light with a built-in neutral studio
environment. The environment supplies diffuse fill and roughness-dependent
reflections, so glossy dielectrics and metals remain readable from more than
one viewing direction. It is lighting-only and is not drawn as a background.

The default display transform uses fixed `0 EV` exposure and Khronos PBR
Neutral tone mapping before one sRGB encoding step. The reference viewer
exposes directional intensity, environment intensity and rotation, exposure,
tone mapping, and the compatibility-only **Legacy ambient** value in the
**Rendering** panel. Automatic exposure and external HDR environments are not
supported.

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
