# Lifetime and Threading

Purpose: Document Elf3D 0.2.0 ownership, destruction order, cache invalidation,
handle validity, and thread-affinity rules.

Applicable version: 0.2.0

Document status: Verified from public headers, facade code, backend code, tests,
and remediation on 2026-06-24.

Last verified Git commit: pending 0.2.0 release source commit

Implementation source paths: `include/elf3d/elf3d.h`, `include/elf3d/scene.h`,
`include/elf3d/viewport.h`, `facade/elf3d/src/engine.cpp`,
`modules/scene/src/scene.cpp`, `modules/backend_opengl/src/device.cpp`,
`tests/public_api_test.cpp`

Known limitations: The 0.2.0 runtime model is single-threaded for scene
mutation and rendering. No background loading, render thread, or task system is
implemented.

Related documents: `PUBLIC_API_OVERVIEW.md`, `RENDERING_PIPELINE.md`,
`VIEWPORT_AND_TOOLS.md`, `TESTING.md`

## Ownership Graph

```text
Host application
  owns native window, event loop, OpenGL context, Dear ImGui, frame presentation
  owns Engine
    creates Scene objects returned to host ownership
    creates Viewport objects returned to host ownership
      owns off-screen render target and per-viewport tool/navigation state
Scene
  owns logical entities, hierarchy, transforms, cameras, visibility, CPU assets
Renderer
  owns GPU caches and render-specific representations
```

The host should destroy objects in this order:

1. Stop using viewports.
2. Destroy `Viewport` objects while the compatible OpenGL context is current.
3. Destroy `Scene` objects.
4. Destroy `Engine`.
5. Destroy host graphics context and window.

## Engine

`Engine` owns the graphics device, renderer, picking service, and scene release
state. `Engine::create` requires a current OpenGL context and a procedure
loader. A default-constructed `Engine` has no graphics backend but can create
CPU scenes.

The public contract remains: the creating `Engine` must outlive `Scene` and
`Viewport` objects created from it.

## Scene

`Scene` owns logical data and CPU assets. Handles include scene identity and are
valid only for their owning scene lifetime. Destroyed entity slots are not
reused in 0.2.0.

Scene destruction releases renderer and picking caches while the engine release
state is alive. Goal 4 replaced the previous raw engine-pointer callback with a
private weak release context. If a scene is destroyed after engine teardown, the
release token is expired and cache release becomes a safe no-op. That safer
behavior does not change the documented host ownership order.

## Viewport

`Viewport` owns GPU render targets and per-view interaction state. Destruction
must occur on the owning graphics thread with a compatible current OpenGL
context. A viewport observes a scene and camera during update/render calls; it
does not own them.

Viewports hold shared internal services so their internals can remain valid
while the public `Engine` facade exists. Hosts should still destroy viewports
before destroying the engine and graphics context.

## GPU Resources

GPU resources must be destroyed before the graphics device/context disappears.
The OpenGL backend checks current-context and thread preconditions. If deletion
preconditions are not met, the backend skips GL deletion to avoid invalid GL
calls; this can leak GPU resources in an incorrect shutdown sequence.

## Scene Replacement

Viewer model loading creates a new scene and imports into it. Failed loads keep
the current scene. Successful replacement clears selection, measurement, and
clipping state that belongs to the previous scene.

## Cache Invalidation

- Renderer mesh and texture caches are keyed by scene/asset identity.
- Picking BVHs are keyed by engine, scene, and mesh identity.
- Scene destruction releases caches through the scene release context when the
  engine release state is still alive.
- The lifetime fix in `7957aee` prevents late scene destruction from
  dereferencing a freed engine implementation.

## Threading

0.2.0 assumes single-threaded scene mutation, navigation, picking, viewport
rendering, and resource management. There is no public synchronization contract
for concurrent scene access. Graphics operations are tied to the thread that
owns the current OpenGL context.

Future concurrency work must define cancellation, shutdown, queue ownership,
and cross-thread error reporting before adding background work.

## Validation

`elf3d.public_api_lifetime` includes a smoke path that destroys a default engine
before destroying a scene created from it. Debug and Release CTest passed after
the lifetime remediation. Manual graphics shutdown validation remains required.
