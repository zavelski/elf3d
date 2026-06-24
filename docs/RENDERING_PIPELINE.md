# Rendering Pipeline

Purpose: Document the verified Elf3D 0.2.0 renderer, OpenGL backend, color
policy, caches, and limitations.

Applicable version: 0.2.0

Document status: Verified from renderer/backend source, tests, and validation
on 2026-06-24.

Last verified Git commit: pending 0.2.0 release source commit

Implementation source paths: `modules/graphics`, `modules/backend_opengl`,
`modules/renderer`, `modules/viewport`, `facade/elf3d/src/engine.cpp`,
`modules/renderer/tests/renderer_test.cpp`

Known limitations: Rendering is opaque-only, OpenGL-only, off-screen-only, and
uses one directional light. No shadows, IBL, environment maps, normal maps,
occlusion, emissive, alpha masking/blending, post-processing, or multiple
graphics backends are implemented.

Related documents: `MODULE_MAP.md`, `GLTF_SUPPORT.md`,
`LIFETIME_AND_THREADING.md`, `VIEWPORT_AND_TOOLS.md`

## Architecture

The public `Engine` composes:

- neutral graphics interfaces in `elf3d_graphics`
- the OpenGL 4.1 backend in `elf3d_backend_opengl`
- renderer preparation and caches in `elf3d_renderer`
- off-screen viewport ownership in `elf3d_viewport`

The renderer consumes scene and asset data. It does not own or silently mutate
logical scene state.

## OpenGL Context Requirements

The host creates an OpenGL 4.1 core-compatible context, makes it current, and
passes a generic procedure loader to `Engine::create`. The backend loads GLAD,
verifies a compatible context, and records the owning thread.

Viewport creation, resize, rendering, native texture view access, and
destruction are graphics-thread operations. GPU resources should be destroyed
while the compatible context is still current.

## Off-Screen Render Target

Each viewport owns an off-screen color framebuffer:

- color attachment: `GL_RGBA8`
- depth attachment: `GL_DEPTH_COMPONENT24` renderbuffer
- framebuffer is recreated on resize
- zero width or height releases the current target and produces no color texture

The host presents the viewport color texture through `NativeTextureView`, often
through `elf3d_imgui` in the reference viewer.

Each viewport also owns a private picking framebuffer used only for GPU-first
surface picking:

- ID attachment: `GL_RGBA32UI`
- depth attachment: `GL_DEPTH_COMPONENT24` renderbuffer
- framebuffer is recreated on resize with the color target
- the backend reads one ID/depth pixel for each picking request

## Render Preparation

The renderer builds a render list from visible scene model entities, camera
state, materials, meshes, and clipping filters. Bounds are transformed to world
space. Clipping broad-phase statistics record tested, rejected, and
intersecting bounds.

Negative scaling is handled by model determinant checks and front-face
orientation. Normal matrices are computed from model transforms for lighting.

## Shader Path

The material shader implements a compact opaque metallic-roughness lighting
path:

- base color factor and optional base-color texture
- metallic and roughness factors and optional metallic-roughness texture
- one directional light
- ambient term
- GGX-style specular term
- double-sided normal flip via `gl_FrontFacing`
- selection highlight tint mixed before final encode
- per-fragment section-plane and clipping-box discard

The fragment alpha output is always `1.0`.

## Color Space

Base-color textures are uploaded as sRGB textures. Metallic-roughness textures
are uploaded as linear `GL_RGBA8`. The off-screen target is non-sRGB `GL_RGBA8`;
the shader manually encodes the final linear color to sRGB exactly once.

## GPU Caches

The renderer maintains:

- static GPU mesh cache keyed by scene and mesh identity
- texture cache keyed by scene, image, color-space role, and sampler

Scene destruction releases renderer and picking caches through the private scene
release context. After Goal 4, late scene destruction after engine teardown
does not dereference a freed engine implementation pointer.

## OpenGL State

The backend uses state guards around allocation, draw, clear, and overlay paths.
The draw path restores framebuffer bindings, viewport, clear state, color and
depth masks, selected enable states, program, VAO/VBO, texture bindings for
units used by the renderer, depth function, blend settings, culling, front face,
polygon mode, and depth range.

## Tool Overlays

Measurement and clipping helpers are rendered through neutral overlay line and
point-marker primitives. Overlay primitives are produced by tool controllers,
not by ImGui, and then drawn by the backend.

## Picking Pass

The renderer reuses render-list construction for GPU picking, including
persistent visibility, viewport isolation, material sidedness, front-face
orientation, and clipping filters. Each visible primitive is drawn with a
nonzero object ID and its model primitive index. The picking fragment shader
writes object ID, primitive index, and `gl_PrimitiveID` to the integer target
after applying the same section-plane and clipping-box discard rules as the
visible material shader.

The viewport maps the readback ID to a scene entity, mesh, primitive, and
triangle candidate, then asks the CPU picking service to refine that single
triangle against the public viewport ray. If the GPU path fails or the candidate
cannot be confirmed, the viewport falls back to the full CPU BVH picker.

## Statistics

`RenderStatistics` reports draw calls, triangles, vertices, indices, texture
bindings, GPU texture uploads, current unique GPU texture count, overlay counts,
and clipping broad-phase counters.

`PickingStatistics` reports GPU picking requests, hits, misses, picking draw
calls, pixels read, pass/readback timing, CPU refinements, and CPU fallbacks in
addition to the existing CPU BVH counters.

## Validation

Debug and Release builds and CTest are part of the 0.2.0 release gate. The
exact local, CI, and viewer smoke results are recorded under
`docs/releases/0.2.0/`.
