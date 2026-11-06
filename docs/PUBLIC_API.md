# Elf3D C++ API Guide

`DocumentHierarchySnapshot` owns names, typed IDs, scalar primitive counts and
relationships for both authored-object and mesh-resource browsing. It contains
no geometry, image payload, Document owner or retained Document view. Scene,
node, mesh and primitive enumeration preserves canonical table order. Mesh
users include nodes outside scenes, and unused meshes remain present.
Scene roots plus canonical node links distinguish repeated presentations of a
node across scenes. Nodes expose camera/light flags and authored-scene membership.

`Document::index_of(id)` and `DocumentView::index_of(id)` resolve node, mesh,
primitive and material IDs to zero-based positions in the current canonical
tables. These non-allocating constant-sized checks reject empty, foreign or stale
IDs. Positions are source-table coordinates, never persistent identities or
guaranteed indices in an exported file. Retain the typed ID and source revision
when capturing a position for later work. Queries do not mutate the revision.

Use `Document::hierarchy_snapshot()` for a synchronous capture. Interactive
consumers use `DocumentHierarchySnapshot::create(view)` and explicitly drive
`advance(view, work_units)` on the source's owning thread. No partial views are
exposed. Construction limits each unit to a small record, 4096 name bytes and
64 links; allocator calls and supported-light inspection remain indivisible.
The builder rejects changed revisions, foreign sources and zero budgets.
Completed views borrow the snapshot and remain usable after source destruction.

`SceneHierarchySnapshot` captures the runtime forest, including procedural and
application-only entities. Its synchronous `Scene::hierarchy_snapshot()` wrapper
drives the same builder as `SceneHierarchySnapshot::create(scene)` followed by
`advance(scene, work_units)`. Each unit advances one structural action or copies
at most 1024 name bytes. Metadata capacity and individual name-buffer allocations
remain indivisible; no geometry, pixels, Scene owner or borrowed source view is
retained. Traversal is linear in entity slots, edges and copied name bytes, using
a dense visited table rather than repeated searches through prior entities.

Build on the Scene-owning thread. `ready()` gates readable rows; pending captures
have size zero. `matches_scene()` checks Scene identity, hierarchy revision and
visibility revision. A changed/foreign source or zero budget is rejected without
advancement. Camera motion does not invalidate capture. Completed names and rows
remain owned by the snapshot after renaming or destroying the source Scene.

`Document::revision()` and `DocumentView::revision()` track persistent mutations,
including verified geometry changes and metadata attachment. They are distinct
from Scene/view revisions and comparable only within the same Document owner.
Moving a Document preserves that owner; a moved-from owner has revision zero.

`Scene::entity_for_document_node(NodeId)` and `document_node(EntityId)` map
validated authored identities to the active runtime hierarchy and back. A valid
inactive/detached node or runtime-only entity returns an empty optional. Invalid,
foreign and destroyed IDs return an error. `document_primitive(entity, index)`
resolves a runtime material-group index to a canonical primitive; multiple groups
may share one primitive. The `PickHit` overload also validates its mesh binding.
Runtime indices must never be interpreted as source-file primitive indices.

`PrimitiveInstance` combines a runtime entity and a canonical Document primitive.
Viewport primitive visibility and isolation copy only these typed IDs. They never
modify the Document or affect Save. Local visibility is the view-local mask;
effective visibility also includes the Scene hierarchy and current isolation.
Clipping remains a separate geometric constraint. Invalid, foreign or wrong-mesh
instances reject the complete request without changing accepted state. Empty
primitive isolation clears isolation; it replaces entity isolation and vice versa.
Drawing, CPU/GPU picking, anchors and visible bounds use the same resolved mask.
`fit_to_primitives` respects that mask and clipping without changing either.

`set_selected_primitives` replaces entity/hit selection with normalized canonical
instances. `selected_primitive_count/at` enumerate their owned IDs; the selection
snapshot carries their count/revision for retained-frame invalidation. Selecting
an entity or a picked hit replaces this resource selection. Set
`ViewportRenderOptions::primitive_highlight` to a `SelectionHighlight` to tint
all runtime material groups originating from the selected canonical instances.
The application framework copies these color/strength settings when queuing a
render. Hidden selected instances remain selected and do not draw or pick.

`snapshot_document(DocumentView)` creates fresh owner-scoped IDs and independent
tables, geometry and metadata, while sharing immutable decoded/encoded image
bytes. It has the validation and synchronous borrowing contract of
`clone_document`; the latter continues to allocate fully independent payloads.
After snapshot completion either owner may be destroyed or transferred to a
worker without retaining the other Document or its Scene. Mutating the same
Document concurrently remains unsupported. No GPU cache sharing is implied.

`Document::replace_image(ImageId, ModelImageDescription)` validates and copies
new payloads before committing to this owner. Existing snapshots retain the old
bytes. The ID and typed references survive; this owner's image views expire and
opaque metadata is marked stale. Invalid/foreign IDs, invalid descriptions and
an encoded-only EXR replacement of a material texture fail without mutation.
The supplied description may borrow the image being replaced. Allocation
exhaustion follows the existing fatal policy. Resource mapping and metadata
transformation for an atlas remain application responsibilities.

The standard `Application` lifecycle includes the optional
`close_requested(ApplicationUpdateContext&) -> Result<bool>` callback. It runs
once per platform close request, before `update`, on the framework thread.
Returning false consumes the request and keeps frames running so an application
can present an unsaved-work dialog or wait for cancellation. Returning true
performs normal `stop` and teardown; an error follows normal error teardown.
The default returns true for compatibility. `request_exit()` means the
application has already approved exit and does not invoke this callback.

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

`Document::replace_primitive` invalidates preserved raw metadata by default.
Its explicit `PrimitiveMetadataPolicy::preserve_verified_references` argument
is for callers that have audited every retained metadata scope for this exact
geometry operation. All retained references must remain valid, including
references outside the replaced primitive. The operation keeps object/resource
table identities; it does not remap raw vertex or triangle references.
The flag never clears existing stale metadata and does not bypass geometry,
material-ID or owner validation. Both borrowed and consuming overloads have
the same rule. See `examples/document_roundtrip.cpp` for the checked call shape;
schema compatibility belongs to the consuming application.

A desktop consumer may link both `elf3d::model` and `elf3d::app` (or the
runtime target directly). No consumer `ELF3D_STATIC` definition is needed or
propagated. Runtime classes retain DLL import/export and DLL-owned destruction;
the shared core diagnostic functions can resolve from either product.

Build all Model consumers from matching headers with the same compiler, CRT
and iterator-debug settings. Model values are not a versioned binary SDK.
Document IDs contain an opaque implementation domain and a monotonic owner
sequence, so independently linked Model copies do not share an assumed global
counter. IDs survive a Document move and reject foreign or replaced storage.
They are process-local: do not serialize them or keep them across unloading
the module that created their owner. `debug_value()` is only a local ordinal.
Rebuild all consumers after an ID layout change.

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

`ApplicationUpdateContext::set_window_title()` copies a nonempty UTF-8 title
without embedded NUL and applies it after the update succeeds. The input string
may expire immediately after the call; failed updates do not publish requests.

`Viewport::isolate_entities(scene, roots)` copies validated Scene-owned roots
and displays their subtree union, subject to normal Scene visibility. Empty
input clears isolation. Duplicate roots are counted once; overlapping branches
are visited once. Invalid or foreign roots leave current isolation unchanged.
`isolated_entity_count()` reports the root count; the singular getter returns
the first root. Bounds, rendering and picking share the same filter.

CPU-only model work does not require either lifecycle; link `elf3d_model` and
use the `Document` API directly.

Document construction, cloning, mutation, and validation are `noexcept`. Expected
validation and I/O failures remain `Result` errors or validation diagnostics;
memory exhaustion is process-fatal, not a recoverable error. Scene and Model
SDK boundaries route escaping allocation failures through `fatal_error`.
Standard-library operations that themselves terminate cannot be intercepted
by an outer boundary. Elf3D does not install process-wide exception handlers.
Import and export reject paths containing embedded nulls and report failed
UTF-8/native path conversion as `ErrorCode::invalid_argument`.

## Cloning a Document

```cpp
auto copied = elf3d::clone_document(source.view());
if (!copied) {
    return copied.error();
}
auto candidate = std::move(copied).value();
```

`clone_document(DocumentView)` synchronously returns a complete, independently
owned canonical `Document`. It requires only `elf3d::model`; it performs no file
I/O and has no graphics-thread requirement. Keep the source alive and unmodified
during the call. After return, either owner may be mutated or destroyed without
affecting the other.

The copy preserves every scene and ordered root, hierarchy, transform, shared
mesh, primitive attribute and triangle assignment, material (including optical
properties and atlas placements), image pixels and original encoded bytes,
texture, sampler, perspective camera and camera IBL attachment. It also retains
all owned names, typed metadata, exact opaque JSON text, required extensions,
the preserved-metadata stale state, and authored default-scene identity or
absence. Import reports belong to `LoadedDocument`, not to `Document`; callers
must retain those reports separately when assessing processing safety.

All eight ID families receive fresh owner identity and all typed references are
remapped. IDs from one owner are rejected by the other. Enumeration order is
preserved for scenes, nodes, meshes, primitives, materials, images, textures and
samplers, as are roots, children and mesh primitive membership. To map a source
selection into the copy, capture its public enumeration index and resolve the
same index in the copy. Do not reuse source IDs or infer identity from their
debug values. The checked example `copy_with_node_transform` in
[`examples/document_roundtrip.cpp`](../examples/document_roundtrip.cpp) applies
this pattern to an independent candidate.

A live empty `Document` is cloneable. A default/null view or a view of a
moved-from owner returns `invalid_argument`; a source validation error returns
that code with the first error diagnostic's message. Stale preserved metadata
is a warning and is copied with its stale state intact. Allocation failure
follows the Model fatal-error policy. As with other borrowed Model views, a
dangling view is outside the contract.

Cloning copies opaque JSON without interpreting or repairing it. It does not
make later structural edits safe, restore data discarded at import, or extend
the glTF export subset. Structural edits still mark preserved metadata stale;
applications must apply their preservation policy before processing.

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

Loading paths are UTF-8 strings. `load_scene()` is the single file scene-loading
operation and always returns the structured compatibility report with the
loaded Scene. The `repaired_signed_buffer_layout` diagnostic identifies GLB
files whose overflowed signed buffer fields were recovered from an
unambiguous embedded BIN layout.
`ignored_vertex_attribute` identifies unsupported vertex streams such as custom
attributes and additional color sets; each diagnostic carries the attribute
name and primitive context. Inspect these reports before processing that
requires lossless input. The new enum value is appended in both Model and Scene
reports, preserving existing diagnostic values.

## Creating a Scene from a Document

Link matching-source `elf3d::model` and `elf3d::app` for Model processing plus
the standard runtime. Both products use the same toolchain and shared CRT;
this is not a stable cross-version Model DLL ABI.

```cpp
// On the framework thread; selected_scene belongs to candidate.
auto preview = context.engine().create_scene_from_document(
    std::move(candidate), selected_scene);
if (!preview) {
    // candidate is unchanged, including errors after runtime derivation began.
    report_error(preview.error());
    return;
}
auto scene = std::move(preview).value();
// Success moved candidate; acquire a fresh, read-only view from the owner.
auto canonical = scene->document();
```

Creation validates the complete input and selected `DocumentSceneId`, then
adopts its exact storage/IDs through the existing runtime adapter. No canonical
geometry or image copy, file round trip, or default-scene rewrite occurs.
Unselected scenes/resources remain present. A selected empty scene is valid;
no scene, foreign/stale selection or moved-from input returns an error.
Import diagnostics remain application-owned and must travel with the version.
Runtime construction may allocate derived geometry for material partitioning
or tangent generation; it is not an allocation-free operation.

CPU preparation can run on an application-owned worker without a live Scene or
an Engine observer. Reserve a `PreparedSceneDocument` with
`Engine::create_scene_preparation()` on the framework thread. Move that owner
to the CPU lane and call `prepare(Document&&, selected_scene, options)` or
`load(path, load_options, preparation_options)` there. These operations perform
validation, material partitioning, tangent generation, runtime entity binding,
world bounds and canonical-to-runtime mappings before transfer. They do not
create threads or graphics objects. Preparation errors recover the exact input
Document; file-load errors publish no partial Document.

Move the ready owner back and call `Engine::install_prepared_document()` on the
framework thread. Installation binds already prepared storage in constant-size
work, without geometry/name/entity traversal or GPU uploads. Its reserved future
Scene ID permits all runtime relationships to be constructed before installation.
Only the originating Engine accepts it; a rejected install preserves the owner.
The CPU owner may outlive that Engine and recover its Document, but cannot be
installed in a different Engine. `statistics()` and `world_bounds()` return
prepared values; `document()` is a temporary read-only observation on the current
exclusive owner thread. All observers expire before installation or recovery.

`ScenePreparationOptions::additional_runtime_entities` reserves CPU capacity for
application objects, such as a view camera, without adding canonical records.
Reserve this headroom during preparation to avoid an entity-array reallocation
when adding those objects after installation. The default is zero. Excessive
capacity returns `resource_limit_exceeded` and preserves the input Document.
The existing synchronous in-memory and file factories drive these same
preparation and installation mechanisms on their calling thread.

`Scene::document()` returns `std::optional<DocumentView>`; absence is normal for
procedural or retired Scenes. The view and its children borrow from this Scene
until extraction/destruction. Observe or clone on its owning thread; do not
retain it in a worker while the Scene can be retired. Runtime entity, visibility,
camera or compatibility-asset edits do not change the canonical Document.

`Scene::take_document()` returns the original canonical owner without copying
and retires the Scene. Its ID becomes invalid, queries return empty/error state,
and further mutations are rejected. All runtime handles, anchors and borrowed
views expire. Renderer/picking caches are released through the normal Scene
release ticket; the transferred Document/IDs survive Scene and Engine teardown.
Rebind/release dependent viewport state before use with another Scene. Calling
take on a procedural/already-retired Scene returns `invalid_argument` without
changing it. Taking requires the live creating Engine/context and owning thread.

`Scene::retire_cpu_state()` instead transfers the complete canonical and derived
CPU state into a move-only `SceneCpuRetirement`, without copying its payloads.
Release dependent viewport observers first. Retirement invalidates the Scene,
its runtime IDs and all borrowed views, and releases context-owned caches through
the normal release ticket on the calling thread. The CPU owner contains no live
Scene, Engine or graphics resources. After transfer it may be destroyed on an
application-owned CPU worker while the producing libraries remain loaded.
`take_document()` on that owner recovers the original canonical Document and
disposes the other derived CPU state on its calling thread. A procedural Scene
can also be retired; extracting a Document from that owner returns
`invalid_argument` without consuming it. An already-retired Scene rejects a
second retirement. This mechanism does not create threads or schedule graphics
retirement; cache release at the Scene boundary is currently synchronous.

The factory prepares CPU runtime data. GPU uploads occur lazily when rendering;
applications must successfully prepare/render a replacement before committing
their own current/history state. If GPU preparation fails after adoption,
take the candidate Document back while the old accepted Scene remains intact.
Different Scene identities have separate texture caches: recreation does not
promise texture reuse. History, worker scheduling and memory admission belong
to the application. Full `clone_document` remains an independent deep copy.

## Exporting a Retained Document

```cpp
auto saved = scene->export_loaded_document("copy.glb");
if (!saved) {
    report_error(saved.error());
}
```

`Scene::export_loaded_document()` exports the canonical `Document` retained by a scene
loaded from glTF/GLB or transferred from memory. The target extension selects `.glb` or `.gltf`; glTF may
create buffer and image sidecars. Runtime visibility, viewport mechanism state, and
Scene-created compatibility assets are intentionally not export data. A
procedural Scene therefore cannot be exported through this operation. The
retained Document is immutable through Scene. For complete write diagnostics,
use `save_document(path, *scene.document())` through Model after checking that
the optional view exists. The convenience export returns only success/error;
it does not expose write warnings from a previously edited input Document.

For application-owned asynchronous Save, include `elf3d/model_write.h`. Capture
a small `ModelWriteSource` on the Document's owning thread, then create and drive
a move-only `ModelFileWriter` on an exclusive CPU owner. This API creates no
threads. Capture stores identity, revision and counts; it owns no Document,
geometry, pixels or text. Each `source.read(current_document, request, bytes)`
borrows the current view for that call only and rejects a changed or foreign
source. Keep the source stable until all requested input is supplied; no source
view may cross into asynchronous work.

When `writer.input_request()` has a value, supply it to the source owner and
return owned packet bytes through `writer.provide()`. The destination must hold
at least `model_write_packet_capacity` bytes; larger storage batches bounded
reads up to `model_write_packet_limit` (2 MiB). Packets are process-local values,
not a file format. `provide()` retains no input span. Otherwise call `advance()`
on the writer owner. Validation, image codecs, binary/JSON serialization, staging
and commit run there. Progress with `total == 0` is indeterminate. Some codec and
file commit calls are indivisible; source transfers and output staging are
subdivided without a complete GLB buffer or a second populated Document.

Image coherence checks finish the current pixel pass before restarting for PNG
fallback. A mismatch inside a batched input packet therefore does not invalidate
the packet's remaining frames. The new PNG contains the current pixels;
unchanged images retain their original encoded streams.

Request cancellation with `advance({.cancel_requested = true})`. Before commit,
staging is discarded. An entered commit completes or recovers synchronously;
late cancellation preserves its actual outcome. After `ready()`, take the
outcome once and inspect `committed`, `cancelled`, `error`, `cleanup_error` and
the write report separately. Multi-file glTF publication is not crash-atomic;
failed rollback preserves its recovery backups. Explicit `discard()` releases
staging and reports cleanup errors. `save_document()` drives the same writer
synchronously for callers that do not need an application worker.

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
  bounds/statistics, validation, complete independent cloning through
  `clone_document()`, and topology-changing primitive replacement.
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

An interaction region registered with `enabled = false` receives empty routed
input. Finalization releases that region's focus and active capture with
`focus_loss`, and requests against it are rejected. This lets a modal suspend
viewport navigation without passing its mouse or keyboard gestures through.

Identical model/runtime POD vocabulary is declared once in
`elf3d/model_types.h`. This includes `AlphaMode`, `PixelFormat`,
`PerspectiveCameraDescription`, texture mapping and sampler values, and
`ModelLoadOptions`. Document-specific DTOs remain distinct where they contain
document-scoped IDs or additional persistent data.

Editors can set `ModelLoadOptions::preserve_unreferenced_data` to retain
supported detached node forests, meshes and complete image/sampler/texture/
material tables. Authored scene roots and the default scene do not change;
detached nodes do not become visible scene members. Default loading continues
to import reachable geometry and its resources. The opt-in mode validates and
budgets the additional data as ordinary imported data, and fails if a declared
image or texture cannot be retained. It does not promise support for animation,
skins, unreferenced camera definitions, or unknown extension semantics.
Existing fallback/loss diagnostics still apply. Use the same option when
reloading an editor export for semantic verification. See `load_for_editing`
in the compile-checked `examples/document_roundtrip.cpp`.

Editors can additionally set `ModelLoadOptions::allow_empty_geometry` to load a
valid document with no supported triangle primitives, including a saved result
of removing all geometry. Other validation and diagnostic rules remain in
effect. The default remains a geometry-required import. Child order follows
the authored `children` arrays, independently of node-table order.

For complete reconstruction, build a fresh `Document` using the ordinary
typed builders. `create_image_snapshot(source, image)` creates a fresh image
record sharing only immutable payload bytes; it copies no opaque metadata and
either Document may be destroyed first. `copy_verified_metadata(source, mapping)`
then atomically attaches the source root/asset/required-extension declarations
and explicitly mapped object/camera metadata. IDs are checked in their owners;
duplicate destinations, foreign IDs, stale source metadata, an already annotated
target, and mismatched camera presence fail without target mutation. Multiple
targets may inherit one source; unlisted targets receive no metadata.

The caller must first certify that opaque references remain valid or were
deliberately handled by its operation policy. This API does not remap arbitrary
JSON, infer identity from names/ordinals, or make unknown extensions safe.
Build all typed fields, cameras and resources before attaching metadata; later
structural mutations follow the ordinary stale-metadata rules. The mapping and
source are borrowed for the call only. Allocation failure remains fatal.

`attach_verified_metadata(ModelDocumentMetadata)` serves reconstruction after
the source Document has already been destroyed. The owned value contains
root/asset/required declarations and metadata paired with the fresh target's
scene/node/camera/mesh/primitive/resource IDs. It is consumed synchronously,
without a source Document, a history owner or an implicit identity map.
The caller certifies JSON syntax and the meaning of every retained opaque
reference before calling. The operation checks the existing bounded envelope
(1 MiB per JSON block, 64 MiB total), names, owner-scoped IDs, duplicate
destinations and camera presence, then attaches atomically. It rejects a stale,
invalid or already annotated target and never clears stale state. It does not
parse, rewrite or infer references in JSON. Complete typed construction before
attaching; later structural changes follow the normal invalidation rules.
The checked external Model SDK example exercises this operation on a fresh
document with literal, nonreferential JSON.

All exported functions, destructors, and callbacks are explicitly `noexcept`.
Memory exhaustion is fatal by default and is not reported as a recoverable
`Result`.

Imported images retain optional original PNG/JPEG bytes and MIME; export reuses
them while they still decode exactly to the current pixels and otherwise
reports PNG re-encoding. Bounded raw glTF `extras` and unknown extensions are
available as read-only metadata views. Node-matrix edits retain these owned
values unchanged. Structural/resource edits mark the complete preserved set
stale, so validation warns and export omits it. Unknown source indices and spatial
semantics are not generally remapped or transformed. The importer uses the
same canonical attachment implementation. The explicit verified transfer and
owned construction operations above support audited reconstruction; neither
is a general JSON-reference remapper. Image placement uses
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
- [`document_scene.cpp`](../examples/document_scene.cpp)
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

## Explicit Scene graphics preparation

`Engine::advance_scene_graphics(scene, budget)` drives persistent preparation of
that Scene's geometry, material images and draw metadata on the owning context
thread. The operation retains IDs, revisions, counters and exclusive GPU owners,
not Scene/Document/pixel observers. A call borrows current source views only while
advancing a bounded CPU/native unit. `Engine::scene_graphics_status(scene)` is a
constant-size query. Foreign Scene owners are rejected.

The budget limits elapsed service time between units, uploaded bytes, actual
reported native calls and work units. Shared defaults are 20 ms, 256 MiB of
uploads, 65536 calls, 65536 work units and 4096 retired objects. CPU packing
uses 8 MiB portions and individual transfers use 32 MiB portions by default;
`packing_bytes` and `transfer_bytes` allow explicit smaller or larger portions.
These defaults apply to SDK consumers and both applications. A texture
allocation/upload unit can need 24 calls including state
capture/restoration; an eight-call allowance cannot admit that unit. These are
scheduling allowances, not latency guarantees on an individual driver call.
The returned activity records per-call totals plus the last and longest step,
with typed EntityId, runtime primitive index and optional material texture slot.
These identities describe runtime instances; a runtime primitive index must not
be interpreted as a glTF primitive index. Allocation values describe logical
base storage, not exact physical VRAM.

An admitted incomplete or stale Scene cannot render or pick, including CPU
picking. Keep render content stable during preparation. After editing an already
ready Scene, advance again: immutable GPU resources are reused and old draw
metadata is removed in bounded units. Repeated advancement of an unchanged ready
Scene performs no upload. Existing consumers which never admit explicit
preparation retain the synchronous lazy path. The document_scene example shows
how to drive preparation during framework updates.

This contract currently covers Scene model geometry/material resources. Shared
shader pipelines and built-in environment resources retain their separate lazy
lifecycle; Studio frame integration, shared-resource scheduling and bounded
retirement are still required by the evolution plan. Do not treat this API's
model-resource readiness as evidence that those application workflows are done.

## Deferred graphics and picking cache retirement

Scene CPU retirement now detaches Renderer and picking caches by transferring
owned map nodes. It does not traverse every GPU resource or BVH on the releasing
thread. Retired caches retain no Scene/Document/pixel views. BVH bounds, triangle
order and nodes are owned arrays; they remain safe after source CPU destruction.
The canonical CPU owner is still `SceneCpuRetirement`, which the application can
release on its exclusive worker independently of this service.

Drive `Engine::advance_graphics_retirement(budget)` on framework updates,
including frames without an accepted model, and before final context teardown.
One work unit deletes at most one native object, releases one BVH/staging array
allocation, or removes fixed cache metadata. The budget limits service time
between units, unit count, native calls and deleted objects. One OpenGL object
deletion reports three calls including context validation and error checking.
An individual allocator/driver call can exceed the time target; last/longest
unit measurements identify the phase and retired Scene ID. The status separates
Renderer and picking queues, logical cache entries, graphics estimates and owned
CPU buffer bytes. Graphics estimates are not exact physical VRAM usage.

`Engine::advance_scene_graphics()` first advances these queues, subtracting their
elapsed time, calls and units from the same supplied allowance. It starts no
new model GPU allocation while a prior retirement remains queued. The report
contains retirement and preparation activity separately; check both when
accounting for a frame. A positive admitted request remains unavailable to render
or pick even if retirement consumes that call's entire budget. An already ready
unchanged Scene can still render while unrelated retired owners are pending.
Existing synchronous cache-miss consumers use the same retirement units through
a blocking completion wrapper. Without explicit service calls, destruction is
therefore deferred until that wrapper or final Engine teardown; applications
seeking bounded cleanup must drive the explicit service.

The Engine supports several independently owned Scenes and retirement groups.
Applications can impose a stricter single-generation admission rule and drive
this service during their closing transition. These APIs do not create a
thread, shared context, history Document or task scheduler.


## Shared environment during Scene preparation

For a nonempty Scene, explicit `advance_scene_graphics` preparation now includes
validation and upload of the built-in diffuse/specular environment and BRDF LUT.
The source is reborrowed within each call; checksum work covers the fixed
1,622,000-byte payload per unit and pixel requests use the same bounded cube/2D drivers as synchronous
creation. No resource pixels or Scene/Document views survive an advancement.
An empty Scene need not start environment preparation. Global environment work
can be reused by later Scenes without retaining their model owners.

`SceneGraphicsPreparationStatus::environment_ready` describes the shared set;
`shared_environment_bytes` reports its logical allocated storage separately
from that Scene's model `allocated_bytes`. Activity still includes all serviced
uploads and calls, including shared work. Shared allocation/upload steps identify
`EnvironmentTextureKind` and, for cube transfers, mip level and face. These are
backend-neutral coordinates and estimates, not native graphics IDs or exact
physical VRAM usage. Cube faces are ordered +X, -X, +Y, -Y, +Z, -Z; mip levels
start at zero. Readiness prevents a first render from lazily uploading
these environment textures; shader preparation remains a separate unfinished
transition and is not implied by this status.

A failed shared transaction retains its exclusive preparations for bounded
cleanup, including already ready members. `GraphicsRetirementStatus` reports
`shared_preparations` and `shared_texture_bytes`; `empty()` includes that queue.
Its `environment_object` step has no Scene ID because the source is Engine-owned.
The normal Engine retirement service advances it even when no model is accepted.
After cleanup, a new Scene admission may retry; a failed Scene does not silently
become ready. Synchronous compatibility calls drive the same cleanup units before
returning the failure. Accepted global resources remain cached independently of
model retirement.


Explicit preparation of a nonempty Scene now includes both core and extended PBR
programs before the Scene reports ready. `pipelines_ready` describes this Engine
cache independently of `environment_ready`; an empty Scene need not warm either.
Shader activity identifies `ShaderPipelineKind` and separates vertex compilation,
fragment compilation, linking and binding work. Driver compilation/linking calls
are indivisible; the budget is checked between these named units. A failed
incomplete program joins shared retirement and deletes at most one native object
per unit. Existing synchronous rendering drives the same preparation mechanism.
Picking, overlay and display-resolve shader warmup are separate remaining work;
this status does not yet promise that every first-use graphics path is prepared.


Explicit nonempty Scene preparation also warms picking, overlay and display
resolve programs before readiness. `auxiliary_graphics_ready` describes this
context-wide cache separately from the main PBR programs. `ShaderPreparationPhase`
identifies the native work unit, including source submission, compile status,
link status, each binding and vertex object setup; activity carries both kind
and step. The earlier main-only warmup limitation is superseded by this path.
Display resolve shares its fixed program/vertex array within the owning context;
each render target keeps independent HDR/display textures, extent, exposure,
tone mapping and retained-frame state. A target never keeps an asynchronous
Scene/Document observer. Existing synchronous consumers drive the same units.


A failed `advance_scene_graphics` still returns its original `Error`. Query
`scene_graphics_status` before disposing the Scene: optional `failure` owns the
failed resource/phase, elapsed milliseconds and admitted driver-call limit.
Shader failures retain the exact shader step; image/environment failures retain
their texture or mip/face location. This context contains only values and IDs.
It does not retain a Scene, Document, pixels or geometry. The call limit is an
allowance, not an exact count of calls made before the failure.

`graphics_retirement_status().last_failure` similarly preserves the last failed
retirement unit's Scene ID, phase, duration and admitted call allowance. It may
remain after successful cleanup: `empty()` describes pending owners and ignores
this diagnostic value. Studio/Viewer capture both status snapshots when an
advance fails. No hidden logging or change to normal error propagation is added.


### Framing selected object branches

`Viewport::fit_to_entities(scene, camera, roots)` frames the visible, unclipped
union of the requested runtime entities and their descendant branches. It also
works with procedural Scenes that have no retained Document. It intersects the
request with current entity isolation and primitive masks, without changing
selection, isolation or Document data. Duplicate/overlapping roots are normalized.
Empty, invalid or fully excluded requests fail without moving the camera.
Resource-instance callers continue to use `fit_to_primitives`.

`Viewport::set_backface_culling_enabled()` is view-local and defaults to enabled.
Enabled respects each material's sidedness; disabled draws both faces without
changing the Document. The effective setting also governs GPU/CPU picking and
navigation focus depth, and invalidates retained frames only when changed.
Explicit `PickOptions::respect_material_sidedness = false` permits a two-sided
query even when normal view rendering culls back faces.


Completed framework frame statistics also expose
`ApplicationFrameStatistics::presentation_completed_seconds`: seconds since the
process's `std::chrono::steady_clock` epoch captured after presentation returns.
It allows application operation timestamps in the same clock domain to end at
the completed frame instead of the next update callback. It is monotonic timing,
not a calendar timestamp. Failed and exit-only frames do not publish statistics.
