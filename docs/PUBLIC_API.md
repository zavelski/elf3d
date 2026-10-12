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
available as read-only metadata views. Node-matrix edits retain these owned
values unchanged. Structural/resource edits mark the complete preserved set
stale, so validation warns and export omits it. Unknown source indices and spatial
semantics are not generally remapped or transformed. The attachment bridge is
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

## Console Model SDK example

[`elf3d_cmd`](CMD.md) demonstrates `load_document`, direct `Document` mutation,
`validate_document`, and `save_document` through public headers and `elf3d::model`.
Its rotation source is application-owned, suitable for copying and adapting.
[`external_model`](../examples/external_model) builds copied source independently
with graphical products disabled. The operation uses existing Document mutation APIs.

`ModelWriteOptions::numeric_policy` defaults to
`ModelNumericWritePolicy::round_trip`, using shortest binary32 decimal tokens.
`clean_rotation` additionally cleans near-integer orthonormal matrix-basis
coefficients within `1e-7`, reports `matrix_rotation_cleaned`, and never changes
the source Document. Translation, scaled/sheared bases, geometry, and opaque
metadata numbers are excluded. Both policies share the normalized JSON layout;
see [GLTF.md](GLTF.md) for formatting, bounds, and fidelity limits.

## Extended material descriptions

`ModelMaterialDescription` uses owner-scoped `TextureId` values. The scene
convenience `MaterialDescription` uses `TextureAssetHandle` values owned by the
same Scene. Both expose optional transmission, volume, and clearcoat descriptions,
plus specular-strength and specular-color texture bindings and independent
`TextureMapping` values. Absence of an optical description preserves ordinary PBR.

Transmission/clearcoat default to zero; clearcoat normal scale defaults to one.
The twelve slots follow `MaterialTextureSlot` order. All slots support UV0/UV1
and transforms within the existing image-format policies. Strength uses alpha,
specular color uses sRGB RGB, transmission/clearcoat use linear red, and thickness/
clearcoat roughness use linear green. Extension slots use ordinary texture bindings.
IOR defaults to 1.5; zero is the defined infinite-IOR compatibility case.

`RenderStatistics::transmission_scene_copies` counts scene-color copies in the
current geometry render. `estimated_resident_transmission_bytes` reports the
current viewport's allocated RGBA16F mip chain, including when a later ordinary
scene reuses that viewport. Public structure layouts changed; rebuild consumers.
Existing fields and their neutral aggregate-initialization order are retained.

## Optical materials and required-extension retention

`ModelMaterialDescription::volume` optionally owns `ModelVolumeDescription`:
nonnegative finite `thickness_factor`, an owner-scoped `thickness_texture` and
its mapping, `attenuation_color` in [0, 1], and an optional positive finite
`attenuation_distance`. Absence of the distance represents infinite attenuation
(the glTF default). The thickness texture follows the existing UV0/UV1 and
missing-UV fallback contract. Import and normalized export retain these values.
Standard rendering evaluates Beer-Lambert attenuation through a local parallel
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

`ModelMaterialDescription::name` is an input string view copied into Document
storage by `create_material`. Returned material descriptions borrow that owned
name; lookups perform no string allocation. Copy the name into an owned string
when an independent material value must outlive the Document, and discard
borrowed views before mutation or Document destruction.
