# Elf3D C++ API Guide

Elf3D exposes its Runtime and Model SDK C++20 API through the headers in
`include/elf3d`. The primary Runtime SDK include is:

```cpp
#include <elf3d/elf3d.h>
```

Focused runtime users may include `graphics.h` for backend and overlay
vocabulary, `rendering.h` for render configuration and statistics, and
`viewport.h` for the caller-owned viewport facade. The umbrella header provides
all three. Standard desktop applications additionally include:

```cpp
#include <elf3d/app/application.h>
```

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

## Choosing the Lifecycle

`elf3d_app` / `elf3d::app` is the canonical desktop path. An application
implements the narrow synchronous `Application` lifecycle and transfers control
once with `run_application()`. The framework owns the window, OpenGL context,
events, normalized input, frame order, queued rendering, presentation, shutdown,
and teardown. `ApplicationContext::engine()` provides borrowed runtime access
during valid lifecycle phases.

```cpp
class MyApplication final : public elf3d::Application {
  public:
    elf3d::Result<void> start(elf3d::ApplicationContext& context) noexcept override;
    elf3d::Result<void>
    update(elf3d::ApplicationUpdateContext& context) noexcept override;
    elf3d::Result<void>
    build_ui(elf3d::ApplicationUiContext& context) noexcept override;
    void stop(elf3d::ApplicationContext& context) noexcept override;
};

MyApplication application;
elf3d::ApplicationOptions options;
options.title = "My Elf3D Application";
auto exit_result = elf3d::run_application(options, application);
```

All graphical applications link `elf3d::app` and use `run_application()`.
The framework owns the complete native lifecycle, including hidden utility
applications. The Runtime SDK exposes no public Engine factory or native
texture presentation interface.

CPU-only model work does not require either lifecycle; link `elf3d_model` and
use the `Document` API directly.

Document construction, mutation, and validation are `noexcept`. Expected
validation and I/O failures remain `Result` errors or validation diagnostics;
memory exhaustion is process-fatal, not a recoverable error. Scene and Model
SDK boundaries route escaping allocation failures through `fatal_error`.
Standard-library operations that themselves terminate cannot be intercepted
by an outer boundary. Elf3D does not install process-wide exception handlers.
Import and export reject paths containing embedded nulls and report failed
UTF-8/native path conversion as `ErrorCode::invalid_argument`.

## Loading a Scene

```cpp
auto loaded_result = context.engine().load_scene("model.glb");
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
loaded Scene. The `repaired_signed_buffer_layout` diagnostic identifies GLB
files whose overflowed signed buffer fields were recovered from an
unambiguous embedded BIN layout.

## Exporting a Loaded Document

```cpp
auto saved = scene->export_loaded_document("copy.glb");
if (!saved) {
    report_error(saved.error());
}
```

`Scene::export_loaded_document()` exports the canonical `Document` retained by a scene
loaded from glTF/GLB. The target extension selects `.glb` or `.gltf`; glTF may
create buffer and image sidecars. Runtime visibility, viewport mechanism state, and
Scene-created compatibility assets are intentionally not export data. A
procedural Scene therefore cannot be exported through this operation. The
retained imported Document is immutable through Scene, so the current
Document-only write diagnostics are not reachable through this facade bridge.

## Creating and Rendering a Viewport

```cpp
auto viewport_result = context.engine().create_viewport({1280, 720});
if (!viewport_result) {
    report_error(viewport_result.error());
    return;
}
auto viewport = std::move(viewport_result).value();

viewport->set_environment_lighting({2.0F, 0.0F});
viewport->set_display_transform({0.0F, elf3d::ToneMappingMode::standard});

auto camera_result = scene->create_perspective_camera_entity({});
if (!camera_result) {
    report_error(camera_result.error());
    return;
}
const elf3d::EntityId camera_entity = camera_result.value();

auto queued = ui_context.queue_viewport_render(*viewport, *scene, camera_entity);
if (!queued) {
    return queued.error();
}
```

The framework executes queued rendering after application UI participation,
resolves the display image, and presents the final frame. Direct
`Viewport::render()` remains a low-level operation within the framework-owned
graphics lifetime, used by focused backend tests.

`RenderStatistics` reports primitive visibility, passes, draw/resource work,
resident-byte estimates, CPU phases, and delayed nonblocking GPU main/resolve
timings. `PickingStatistics` reports the corresponding picking pass, readback,
allocation, CPU, and delayed GPU timing. A GPU value remains unavailable until
an older timer query completes; rendering never waits for the current query.
Applications may select diagnostic `RenderShadingMode::unlit`, retain a rendered
texture until `Scene::revision()` or `Viewport::render_revision()` changes,
and keep the default standard PBR path unchanged.

`EnvironmentLighting` controls the renderer-owned high-contrast studio fill and
reflection intensity plus its world-Y rotation. `DisplayTransform` controls
fixed exposure in EV and selects Standard, PBR Neutral, or the diagnostic
no-tone-map path. The defaults are environment intensity `2`, rotation `0`,
exposure `0 EV`, and Standard. Environment intensity is clamped to `[0, 8]`,
exposure to `[-8, 8]`, and invalid values are sanitized; each effective change
advances `Viewport::render_revision()` without mutating Scene state.

## Frame Statistics and Image Readback

`ApplicationUpdateContext::previous_frame_statistics()` returns no value in the
first update, then the last fully presented frame's zero-based `frame_index`
and `wall_milliseconds`. The interval starts before event processing and ends
after swap returns. Failed frames and updates that request exit do not publish
new samples. The four Application callbacks are unchanged.

`Viewport::read_color_pixels(std::span<std::uint8_t>)` synchronously reads the
last rendered image after the current display transform. Allocate exactly
`extent.width * extent.height * 4` bytes with checked arithmetic; output is
tightly packed RGBA8, top row first. The operation runs on the graphics thread,
may wait for GPU completion, and does not render the scene again. Before the
first successful render or after target recreation it returns
`texture_unavailable`; a mismatched buffer returns `invalid_argument`, byte-size
overflow returns `size_overflow`, and GPU read failure returns
`gpu_texture_readback_failed`. Changing the display transform can resolve the
retained HDR image without another scene render. Read in the next `update`
after queueing a render in `build_ui`.

## Main API Areas

- `Document`: CPU-side ownership of all imported scenes, the optional authored
  default scene, document-scoped IDs, bounded indexed primitives, perspective
  cameras, materials, images, textures, samplers, read-only source metadata,
  bounds/statistics, validation, and topology-changing primitive replacement.
  `load_document()` imports glTF/GLB and `save_document()` exports the supported
  subset in `elf3d_model`; `ModelWriteReport` carries non-fatal fidelity
  diagnostics.
- `Application`: canonical standard lifecycle, normalized `InputSnapshot`
  participation, owner-scoped interaction arbitration, queued rendering, and
  deterministic teardown.
- `Engine`: runtime scene and viewport creation and loading.
- `Scene`: hierarchy, transforms, cameras, model-backed loaded data,
  Scene-created convenience assets, visibility, bounds, surface-anchor
  creation/resolution, statistics, and retained-Document export.
- `Viewport`: rendering, studio environment and display-transform
  controls, navigation, picking, selection, visibility, explicit clipping,
  projection, resolved-anchor visibility, generic overlays, and statistics.
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

Elf3D objects are returned as `std::unique_ptr`. The framework owns the engine,
which must outlive every scene and viewport created from it. Release these
objects in `Application::stop`.

Shutdown in this order:

1. stop viewport operations;
2. destroy viewports while their OpenGL context is current;
3. destroy scenes;
4. destroy the engine;
5. let the framework destroy its graphics context and window.

Scene mutation, loading, rendering, navigation, picking, and graphics-resource
management are used from the owning application and graphics thread.

## Compile-Checked Examples

The canonical examples are compiled by the normal full-engine test build:

- [`standard_application.cpp`](../examples/standard_application.cpp)
- [`load_and_report.cpp`](../examples/load_and_report.cpp)
- [`procedural_scene.cpp`](../examples/procedural_scene.cpp)
- [`picking_and_selection.cpp`](../examples/picking_and_selection.cpp)
- [`document_roundtrip.cpp`](../examples/document_roundtrip.cpp)
- [`multi_viewport.cpp`](../examples/multi_viewport.cpp)

They check every `Result` before value access and are the preferred source for
application integration code.

The standalone [`external_application`](../examples/external_application/main.cpp)
also queues one frame, reads its display pixels and statistics in the next
update, and exits without rendering another frame.
