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

Lighting combines a directional light with ambient illumination. The reference
viewer exposes both values in the **Rendering** panel.

## Transparency

Opaque and masked geometry is rendered before blended geometry. Blended
objects are ordered from back to front using their scene placement. Results are
most predictable for separated transparent surfaces.

## Visibility and Clipping

Scene visibility, viewport isolation, the section plane, and clipping boxes are
applied consistently to display, picking, measurement placement, visible
bounds, and camera fitting.

## Host Integration

Create, resize, render, present, and destroy a viewport while its compatible
OpenGL context is current. The host must not delete or modify a native texture
handle returned by Elf3D.
