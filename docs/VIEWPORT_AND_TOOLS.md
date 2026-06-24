# Viewport and Tools

Purpose: Document viewport state, input, navigation, picking, selection,
visibility, measurement, and clipping behavior in Elf3D 0.2.0.

Applicable version: 0.2.0

Document status: Verified from public headers, tool modules, viewer code, tests,
and validation on 2026-06-24.

Last verified Git commit: pending 0.2.0 release source commit

Implementation source paths: `include/elf3d/viewport.h`,
`include/elf3d/navigation.h`, `include/elf3d/picking.h`,
`include/elf3d/selection.h`, `include/elf3d/measurement.h`,
`include/elf3d/clipping.h`, `modules/viewport`, `modules/navigation`,
`modules/picking`, `modules/tools`, `apps/viewer/src/main.cpp`

Known limitations: Tools are single-selection and one-distance-measurement per
viewport. There are no transform gizmos, undo/redo, multi-selection,
annotations, serialization, rotated boxes, section caps, or scene editing
tools.

Related documents: `PUBLIC_API_OVERVIEW.md`, `RENDERING_PIPELINE.md`,
`LIFETIME_AND_THREADING.md`, `USER_GUIDE.md`

## Viewport Ownership

A `Viewport` is created by `Engine` and owns per-view state:

- render target
- navigation controller
- selection controller
- visibility isolation controller
- distance measurement controller
- clipping controller
- tool overlay buffers

The viewport observes caller-owned `Scene` data. It does not own the scene.

## Input Coordinates

Public picking and input coordinates are viewport render-target pixels:

- origin at top-left
- positive X to the right
- positive Y downward

The viewer maps Dear ImGui image coordinates into viewport pixels, including
high-DPI framebuffer scale and docked panel resizing.

## Interaction Priority

Navigation and tools share the input snapshot. The current behavior is:

- pending left click remains a click until movement exceeds the threshold
- threshold-crossing frame starts orbit without a jump
- left drag orbits
- X + left drag pans
- Z + left drag dollies
- middle drag pans
- right drag pans
- wheel over the 3D image dollies
- plain surface click updates the examine pivot without moving the camera
- Ctrl + surface click selects
- Shift + surface click hides the hit entity
- measurement tool uses visible surface clicks for anchors
- modal dialogs and text input block keyboard camera shortcuts

`Escape` cancels an incomplete measurement first; otherwise it clears
selection in the viewer. `Delete` clears the stored measurement in the viewer.

## Navigation

Orbit navigation supports:

- orbit around a pivot
- pan
- wheel and drag dolly
- dynamic examine-pivot updates from visible surface clicks
- fit to visible content
- reset to a canonical three-quarter view
- configurable sensitivity and vertical orbit inversion
- diagnostics through `NavigationSnapshot`

Navigation is mouse-based in 0.2.0. Touch, gamepad, first-person movement, and
keyboard fly-camera modes are not implemented.

## Picking

Interactive viewport picking is GPU-first with CPU confirmation:

1. The renderer builds the same visibility- and clipping-filtered render list
   used for the visible pass.
2. The OpenGL backend draws visible primitives into a private integer ID
   picking framebuffer and depth attachment.
3. The backend reads exactly one ID/depth pixel at the requested viewport
   position.
4. The viewport maps the ID back to entity, mesh, primitive, and triangle
   candidate data.
5. The picking service refines the reported triangle against the normal
   world-space viewport ray before creating the public `PickHit`.

If the GPU pass cannot run or the reported candidate cannot be confirmed, the
viewport falls back to the CPU BVH picker. A valid GPU miss does not traverse
the CPU BVH.

The CPU fallback path:

1. Builds a world-space ray from the active perspective camera.
2. Filters model entities by persistent visibility and viewport isolation.
3. Applies clipping broad-phase to transformed model bounds.
4. Transforms the ray into mesh local space.
5. Traverses a lazy per-mesh BVH.
6. Accepts the nearest visible, unclipped triangle hit.

Material sidedness can participate in picking through `PickOptions`.

`PickingStatistics` reports both CPU counters and GPU-pick diagnostics:
requests, hits, misses, picking draw calls, pixels read, pass/readback time,
CPU refinements, and full CPU fallbacks.

## Selection and Visibility

Each viewport has one selected entity. 3D picking selects visible renderable
model entities and stores triangle-hit detail. Hierarchy row selection in the
viewer can select transform-only and camera entities without a triangle hit.

Persistent visibility belongs to `Scene`. Hiding an entity changes its local
visibility flag. Effective visibility is inherited from ancestors. Showing the
selected entity also shows its ancestors while preserving descendant hidden
flags. `Show All` restores scene local visibility flags.

Isolation belongs to `Viewport`. It intersects with persistent scene visibility
and does not mutate the scene.

## Distance Measurement

The distance tool stores stable triangle anchors:

- scene
- entity
- mesh
- primitive index
- triangle index
- barycentric coordinates

Snapshots resolve anchors back to current world-space positions and normals, so
transform changes move the measurement with the model. Distance is canonical in
meters and can be displayed as metric or imperial units by the viewer.

Hidden, isolation-excluded, or clipped geometry cannot create new measurement
anchors. Existing anchors remain stored but overlays are suppressed when anchors
are no longer visible.

## Clipping

Clipping state belongs to each viewport:

- one optional world-space section plane
- up to three world-axis-aligned clipping boxes
- enabled boxes use union semantics
- section plane and box union combine by intersection
- boundary points are retained

Rendering, picking, selection, measurement placement, measurement overlay
visibility, visible-bounds queries, fit, and reset consume the same neutral
clipping filter.

Helper overlays draw the section plane and clipping-box wireframes through the
neutral overlay path.

## Reference Viewer UI

The viewer owns the GLFW window, Dear ImGui context, menu, toolbar, docking
host, side panels, and final presentation. The engine does not depend on Dear
ImGui or GLFW.

The viewer initializes Dear ImGui with docking enabled, Droid Sans at
`20.0f * dpi_scale` when `assets/font/DroidSans.ttf` is present, Cyrillic glyph
ranges, and a light Low.3D-inspired panel style. The permanent toolbar below
the main menu uses project-generated PNG icons from `assets/icon/`; LWApp PNG
icons are not copied. The seeded docking layout keeps the 3D view centered,
uses a narrow right column for scene/selection panels, and places diagnostics
and tool panels in a lower-right split instead of a wide bottom band. The
default 3D view clear color is white to match the light reference workspace.

## Validation

Debug and Release tests cover interaction, navigation, picking, selection,
visibility, measurement, clipping, renderer, and viewport lifetime. Release
validation for 0.2.0 records the exact local and CI results under
`docs/releases/0.2.0/`.
