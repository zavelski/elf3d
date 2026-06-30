# Rendering Pipeline

Purpose: Document the verified Elf3D 0.6.0 renderer, OpenGL backend, material
path, caches, and limitations.

Applicable version: 0.6.0

Document status: Living pipeline description verified from renderer/backend
source and tests.

Implementation source paths: `modules/graphics`, `modules/backend_opengl`,
`modules/renderer`, `modules/viewport`, `facade/elf3d/src/engine.cpp`

Known limitations: Rendering is OpenGL 4.1 and off-screen only. Lighting uses
one viewport directional light. There are no shadows, IBL, tangent-space normal
mapping, layered materials, transmission, or order-independent transparency.

Related documents: `MODULE_MAP.md`, `GLTF_SUPPORT.md`,
`LIFETIME_AND_THREADING.md`, `VIEWPORT_AND_TOOLS.md`

## Architecture

The public `Engine` composes neutral graphics interfaces, the OpenGL backend,
renderer preparation/caches, and off-screen viewport ownership. Renderer reads
Scene and Asset data and never owns or silently mutates the logical scene.

## OpenGL Context and Target

The host creates and keeps current an OpenGL 4.1 core-compatible context.
Viewport creation, resize, rendering, native texture access, and destruction
are graphics-thread operations.

Each viewport owns an off-screen non-sRGB `GL_RGBA8` color attachment and a
`GL_DEPTH_COMPONENT24` depth attachment. GPU picking uses a private
`GL_RGBA32UI` identifier target plus depth.

## Vertex and Primitive Path

The material vertex layout is fixed and interleaved:

- position;
- normal;
- `TEXCOORD_0`;
- `TEXCOORD_1`;
- vertex color.

glTF triangle strips and fans are converted to triangle-list indices during
import, so graphics submission remains indexed `GL_TRIANGLES`. Negative model
scales select the correct front-face winding. Normal matrices are calculated
per model primitive.

## Material Shader

The material shader supports:

- base-color RGBA factor, sRGB texture, and vertex color;
- metallic and roughness factors plus linear metallic-roughness texture;
- independent UV0/UV1 selection for each rendered texture slot;
- independent offset, scale, and rotation for each rendered texture slot;
- emissive factor and sRGB emissive texture;
- ambient occlusion red channel and strength;
- IOR and dielectric specular factor/color;
- unlit materials;
- double-sided normal flipping;
- selection highlight;
- section-plane and clipping-box discard.

Normal texture data is imported and preserved but intentionally not sampled.
Correct normal mapping requires imported/generated tangents, handedness, and a
tested tangent-space path; the importer returns a warning instead of applying a
fake normal-map approximation.

The shader calculates in linear space. sRGB textures decode through their
OpenGL internal format, and the final linear color is encoded to sRGB once for
the non-sRGB viewport target.

## Alpha Policy

- `OPAQUE`: factor/texture alpha is ignored and fragment alpha is one.
- `MASK`: fragments below `alphaCutoff` are discarded; retained fragments are
  opaque.
- `BLEND`: simple output-space source-over blending is enabled and depth writes are disabled.
  Blended model primitives are submitted after opaque/masked primitives and
  sorted back-to-front by squared distance from camera to model origin.

This blend policy is deterministic and useful for ordinary separated
transparent objects. It does not solve intersecting transparent meshes or
per-triangle ordering. Picking and CPU triangle queries do not sample material
alpha, so transparent texels remain pickable geometry.

## Texture and Mesh Caches

Static GPU meshes are cached by scene/mesh identity. GPU textures are cached by
scene, image, color-space role, and sampler. The same image may therefore have
separate sRGB and linear GPU representations while several material slots with
the same role share a representation.

Scene destruction releases renderer and picking caches through the private
scene release context. GPU resources must still be destroyed before the host
destroys the current graphics context.

## OpenGL State

The backend guards allocation, clear, material, picking, and overlay state.
Material draws now preserve texture bindings for units 0 through 3 in addition
to framebuffer, viewport, masks, enabled state, program, VAO/VBO, depth,
blending, culling, polygon mode, and depth range.

## Picking and Overlays

GPU picking reuses the visibility/clipping render list and performs CPU
triangle refinement. Measurement and clipping helpers use backend-neutral line
and point-marker primitives. Neither path depends on Dear ImGui.

## Validation

`elf3d.renderer` uses a fake graphics device to verify material values, UV-set
selection, texture-transform values, alpha modes, unlit state, texture color
roles/cache reuse, shader-path presence, draw ordering, clipping, overlays, and
GPU-pick orchestration. Real shader compilation is exercised when the viewer is
launched with a real OpenGL context; automated reference-pixel testing remains
future work.
