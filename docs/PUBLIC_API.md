# Elf3D C++ API Guide

Elf3D exposes a C++20 API through the headers in `include/elf3d`. The primary
include is:

```cpp
#include <elf3d/elf3d.h>
```

Focused hosts may include `graphics.h` for backend and overlay vocabulary,
`rendering.h` for render configuration and statistics, and `viewport.h` for
the caller-owned viewport facade. The umbrella header provides all three.

Build the `elf3d` target and link the resulting shared library and import
library with a compatible Visual Studio C++20 configuration.

The source-integrated static model library is exposed separately through:

```cpp
#include <elf3d/model.h>
```

This canonical include transitively provides the focused document-scoped ID
declarations from `elf3d/model_ids.h`; applications normally include
`elf3d/model.h` directly.

Build and link `elf3d_model` / `elf3d::model` when an application needs the
canonical CPU-side `elf3d::Document` without renderer, backend, viewport, or
viewer targets. This is not a stable DLL ABI; bulk model DTOs may own standard
library storage, while read-only access uses temporary spans and string views.

## Creating the Engine

A rendering engine requires a current OpenGL context and a host-provided
procedure loader:

```cpp
elf3d::EngineConfiguration configuration;
configuration.graphics_backend = elf3d::GraphicsBackend::opengl;
configuration.opengl.load_procedure = load_host_opengl_symbol;

auto engine_result = elf3d::Engine::create(configuration);
if (!engine_result) {
    report_error(engine_result.error());
    return;
}
auto engine = std::move(engine_result).value();
```

CPU-only scene work uses the same factory with `GraphicsBackend::none`:

```cpp
elf3d::EngineConfiguration configuration;
configuration.graphics_backend = elf3d::GraphicsBackend::none;

auto engine_result = elf3d::Engine::create(configuration);
```

Rendering viewports require a configured graphics backend.

## Loading a Scene

```cpp
auto loaded_result = engine->load_scene("model.glb");
if (!loaded_result) {
    report_error(loaded_result.error());
    return;
}

auto loaded = std::move(loaded_result).value();
auto scene = std::move(loaded.scene);
for (std::size_t index = 0; index < loaded.report.diagnostic_count(); ++index) {
    auto diagnostic_result = loaded.report.diagnostic(index);
    if (diagnostic_result) {
        present_diagnostic(diagnostic_result.value());
    }
}
```

Loading paths are UTF-8 strings. `load_scene()` is the single scene-loading
operation and always returns the structured compatibility report with the
loaded Scene.

## Exporting a Loaded Document

```cpp
auto saved = scene->export_loaded_document("copy.glb");
if (!saved) {
    report_error(saved.error());
}
```

`Scene::export_loaded_document()` exports the canonical `Document` retained by a scene
loaded from glTF/GLB. The target extension selects `.glb` or `.gltf`; glTF may
create buffer and image sidecars. Runtime visibility, viewport tools, and
Scene-created compatibility assets are intentionally not export data. A
procedural Scene therefore cannot be exported through this operation. The
retained imported Document is immutable through Scene, so the current
Document-only write diagnostics are not reachable through this facade bridge.

## Creating and Rendering a Viewport

```cpp
auto viewport_result = engine->create_viewport({1280, 720});
if (!viewport_result) {
    report_error(viewport_result.error());
    return;
}
auto viewport = std::move(viewport_result).value();

auto camera_result = scene->create_perspective_camera_entity({});
if (!camera_result) {
    report_error(camera_result.error());
    return;
}
const elf3d::EntityId camera_entity = camera_result.value();

while (host_running()) {
    elf3d::ViewportInput input = translate_host_input();
    auto navigation = viewport->update_navigation(*scene, camera_entity, input);
    if (!navigation) {
        report_error(navigation.error());
        break;
    }
    auto rendered = viewport->render(*scene, camera_entity);
    if (!rendered) {
        report_error(rendered.error());
        break;
    }

    auto texture = engine->native_texture_view(viewport->color_texture());
    if (!texture) {
        report_error(texture.error());
        break;
    }
    present_texture(texture.value());
}
```

The host owns the native window, OpenGL context, event loop, input translation,
and texture presentation.

`RenderStatistics` reports primitive visibility, passes, draw/resource work,
resident-byte estimates, CPU phases, and delayed nonblocking GPU main/resolve
timings. `PickingStatistics` reports the corresponding picking pass, readback,
allocation, CPU, and delayed GPU timing. A GPU value remains unavailable until
an older timer query completes; rendering never waits for the current query.
Hosts may select diagnostic `RenderShadingMode::unlit`, retain a rendered
texture until `Scene::revision()` or `Viewport::render_revision()` changes,
and keep the default standard PBR path unchanged.

## Main API Areas

- `Document`: CPU-side ownership of all imported scenes, the optional authored
  default scene, document-scoped IDs, bounded indexed primitives, perspective
  cameras, materials, images, textures, samplers, read-only source metadata,
  bounds/statistics, validation, and topology-changing primitive replacement.
  `load_document()` imports glTF/GLB and `save_document()` exports the supported
  subset in `elf3d_model`; `ModelWriteReport` carries non-fatal fidelity
  diagnostics.
- `Engine`: scene and viewport creation, loading, and native texture access.
- `Scene`: hierarchy, transforms, cameras, model-backed loaded data,
  Scene-created convenience assets, visibility, bounds, statistics, and
  retained-Document export.
- `Viewport`: rendering, navigation, picking, selection, visibility,
  measurement, clipping, overlays, and statistics.
- `Result<T>` and `Error`: expected operation results and error context.

Identical model/runtime POD vocabulary is declared once in
`elf3d/model_types.h`. This includes `AlphaMode`, `PixelFormat`,
`PerspectiveCameraDescription`, texture mapping and sampler values, and
`ModelLoadOptions`. Document-specific DTOs remain distinct where they contain
document-scoped IDs or additional persistent data.

All exported functions, destructors, and callbacks are explicitly `noexcept`.
Memory exhaustion is fatal by default and is not reported as a recoverable
`Result`.

Imported images retain optional original PNG/JPEG bytes and MIME; export reuses
them while they still decode exactly to the current pixels and otherwise
reports PNG re-encoding. Bounded raw glTF `extras` and unknown extensions are
available as read-only metadata views. Any successful Document mutation marks
that complete preserved set stale, so validation warns and export omits it
rather than risking invalid unknown references. The attachment bridge is
private to the importer/model implementation; the public Document API has no
raw-metadata setter. Image placement uses
`ModelImageWritePolicy::automatic`, `external`, or `embedded`.

## Ownership and Shutdown

Elf3D objects are returned as `std::unique_ptr`. The engine must outlive every
scene and viewport created from it.

Shutdown in this order:

1. stop viewport operations;
2. destroy viewports while their OpenGL context is current;
3. destroy scenes;
4. destroy the engine;
5. destroy the host OpenGL context and window.

Scene mutation, loading, rendering, navigation, picking, and graphics-resource
management are used from the owning application and graphics thread.

## Compile-Checked Examples

The canonical examples are compiled by the normal full-engine test build:

- [`embedded_viewer.cpp`](../examples/embedded_viewer.cpp)
- [`load_and_report.cpp`](../examples/load_and_report.cpp)
- [`procedural_scene.cpp`](../examples/procedural_scene.cpp)
- [`picking_and_selection.cpp`](../examples/picking_and_selection.cpp)
- [`document_roundtrip.cpp`](../examples/document_roundtrip.cpp)
- [`multi_viewport.cpp`](../examples/multi_viewport.cpp)

They check every `Result` before value access and are the preferred source for
application integration code.
