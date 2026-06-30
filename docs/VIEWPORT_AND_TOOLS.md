# Viewport and Tools

Purpose: Document viewport state, input, navigation, picking, selection,
visibility, measurement, and clipping behavior in Elf3D 0.7.1.

Applicable version: 0.7.1

Document status: Verified from public headers, tool modules, viewer code, tests,
and 0.7.1 local validation.

Last verified Git commit: local tag `v0.7.1` after release commit

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
- left drag orbits in the mouse-movement direction
- X + left drag pans
- Z + left drag dollies
- middle drag pans
- right drag pans
- wheel over the 3D image dollies, even if the docked 3D view does not already
  have keyboard/window focus
- plain surface click updates the examine pivot without moving the camera; a
  later wheel zoom realigns deferred off-axis pivot state to the current camera
  direction before changing distance, so quick clicks do not cause delayed
  camera jumps
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
- default orbit drags make the visible model follow mouse movement
- configurable sensitivity and vertical orbit inversion
- diagnostics through `NavigationSnapshot`

Navigation is mouse-based in 0.7.1. Touch, gamepad, first-person movement, and
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
ranges, and a light Low.3D-inspired panel style. Menus, the toolbar, the status
bar, modals, and 3D overlays use the normal viewer font; docked panel titles use
an intermediate `17.5f * dpi_scale` font; panel content uses a
`14.0f * dpi_scale` font. The permanent toolbar below the main menu uses
project-generated PNG icons from `assets/icon/`; LWApp PNG icons are not copied.
Toolbar buttons apply explicit icon tint and button backgrounds for normal,
hover, active, and disabled states. The seeded docking layout keeps the 3D view
centered, docks `Model Information` beside `3D View`, uses a right column for
scene/selection panels, and places rendering controls and tool panels in a
lower-right split instead of a wide bottom band.

The `3D View` window is a clean zero-padding viewport area. The render texture
fills the available dock content area, and viewport errors are drawn as small
overlays instead of layout text. Source/format/path information remains in
`Model Information`, which uses a white panel background. The Open command uses
an ImGui file browser with Blender-like top navigation, sidebar bookmarks,
system locations, recent folders, search, file metadata, Windows drive shortcuts
when available, and `.gltf`/`.glb` file filtering. Clear color, lighting, procedural
cube color, rotation, speed, and reset transform controls are in the side
`Rendering` panel. The default 3D view clear color is white to match the light
reference workspace.

The About dialog is positioned with Dear ImGui next-window positioning before
the dialog is first rendered, so first-open and later-open centering use the
main application viewport size instead of a previous-frame window size.

Successful model loads retain the public `SceneLoadReport`. The Model
Information panel lists every import diagnostic and its source context, so
optional-extension and visual-fallback warnings do not depend on a console.
Visible alpha-masked texels are discarded by rendering, but picking remains
triangle-based and does not sample material alpha.

## Validation

Debug and Release tests cover interaction, navigation, picking, selection,
visibility, measurement, clipping, renderer, and viewport lifetime. Navigation
tests include regression coverage for hover-wheel zoom without focus and wheel
zoom after a click-derived pivot. The 0.7.1 compatibility paths are covered by
importer, renderer, public API, and viewer builds. Versioned release records
before 0.7.1 remain immutable historical snapshots.
