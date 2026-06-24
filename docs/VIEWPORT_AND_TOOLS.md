# Viewport and Tools

Purpose: Document viewport state, input, navigation, picking, selection,
visibility, measurement, and clipping behavior in Elf3D 0.1.0.

Applicable version: 0.1.0

Document status: Verified from public headers, tool modules, viewer code, tests,
and validation on 2026-06-23.

Last verified Git commit: `8504068`

Implementation source paths: `include/elf3d/viewport.h`,
`include/elf3d/navigation.h`, `include/elf3d/picking.h`,
`include/elf3d/selection.h`, `include/elf3d/measurement.h`,
`include/elf3d/clipping.h`, `modules/viewport`, `modules/navigation`,
`modules/picking`, `modules/tools`, `apps/viewer/src/main.cpp`

Known limitations: Tools are single-selection and one-distance-measurement per
viewport. There are no transform gizmos, undo/redo, GPU ID-buffer picking,
multi-selection, annotations, serialization, rotated boxes, section caps, or
scene editing tools.

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
- Shift + left drag pans
- middle drag pans
- wheel over the 3D image dollies
- measurement tool uses visible surface clicks for anchors
- select tool uses visible surface clicks for selection
- modal dialogs and text input block keyboard camera shortcuts

`Escape` cancels an incomplete measurement first; otherwise it clears
selection in the viewer. `Delete` clears the stored measurement in the viewer.

## Navigation

Orbit navigation supports:

- orbit around a pivot
- pan
- wheel dolly
- fit to visible content
- reset to a canonical three-quarter view
- configurable sensitivity and vertical orbit inversion
- diagnostics through `NavigationSnapshot`

Navigation is mouse-based in 0.1.0. Touch, gamepad, first-person movement, and
object-focus commands are not implemented.

## Picking

Picking is CPU-based:

1. Build a world-space ray from the active perspective camera.
2. Filter model entities by persistent visibility and viewport isolation.
3. Apply clipping broad-phase to transformed model bounds.
4. Transform the ray into mesh local space.
5. Traverse a lazy per-mesh BVH.
6. Accept the nearest visible, unclipped triangle hit.

Material sidedness can participate in picking through `PickOptions`.

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

## Validation

Debug and Release tests passed for interaction, navigation, picking, selection,
visibility, measurement, clipping, renderer, and viewport lifetime. Manual
viewer interaction remains unverified.
