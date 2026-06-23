# Elf3D

Elf3D is a portable C++20 3D visualization engine intended to be embedded as a
shared library in desktop host applications. The current rendering stage adds a
Scene and static-mesh asset API, perspective cameras, transform hierarchies,
and opaque metallic-roughness PBR shading through the OpenGL 4.1 off-screen
viewport used by the Dear ImGui reference viewer. The glTF vertical slice loads
static textured triangle geometry from `.gltf` and `.glb` files synchronously.

## Initial targets

- `elf3d_core`: minimal internal static library containing version and result data.
- `elf3d_math`: internal GLM-backed conventions and value conversion.
- `elf3d_assets`: validated in-memory position/normal meshes and basic materials.
- `elf3d_image`: private bounded PNG/JPEG decoding into Elf3D-owned RGBA8 pixels.
- `elf3d_scene`: entities, hierarchy, transforms, models, cameras, and scene-owned assets.
- `elf3d_clipping`: neutral section-plane, clipping-box, point-filter, and bounds math.
- `elf3d_interaction`: private viewport input transition and pointer-capture state.
- `elf3d_navigation`: private orbit, pan, dolly, fit, reset, and navigation diagnostics.
- `elf3d_picking`: private CPU ray construction, scene-instance picking, and per-mesh BVH cache.
- `elf3d_tool_selection`: private single-selection state and click-versus-drag selection handling.
- `elf3d_tool_visibility`: private viewport isolation state and shared visibility-filter creation.
- `elf3d_tool_measurement`: private point-to-point measurement state, stable triangle anchors,
  unit conversion, and neutral overlay primitive generation.
- `elf3d_tool_clipping`: private per-Viewport clipping state, commands, visible clipped bounds,
  and helper-overlay generation.
- `elf3d_gltf`: private cgltf-based static glTF 2.0 importer.
- `elf3d_graphics`: minimal internal device and render-target abstraction.
- `elf3d_backend_opengl`: private GLAD/OpenGL 4.1 implementation.
- `elf3d_renderer`: render-list preparation, PBR shader pipeline, static GPU mesh/texture caches,
  and statistics.
- `elf3d_viewport`: off-screen viewport state and render-target ownership.
- `elf3d`: public shared library and Pimpl-based engine/viewport facades.
- `elf3d_imgui`: optional static Dear ImGui GLFW/OpenGL3 integration helper.
- `elf3d_viewer`: standalone reference application and graphical testbed.
- Public API, math-convention, and viewport-lifetime tests.

The engine library has no dependency on Dear ImGui or GLFW. GLM is internal to
`elf3d_math`, while GLAD and OpenGL types remain private to
`elf3d_backend_opengl`. The host application owns the window, event loop,
OpenGL context, Dear ImGui context, GUI, and frame presentation.

## Documentation

The verified technical documentation set starts at `docs/README.md`. The living
project-state baseline is `PROJECT_STATE_EN.md`. Documentation maintenance
rules are defined in `docs/DOCUMENTATION_POLICY.md` and
`docs/DOCUMENTATION_UPDATE_CHECKLIST.md`. The 0.1.0 release-candidate snapshot
is recorded under `docs/releases/0.1.0/`; it is not tagged because manual visual
viewer validation remains incomplete.

## License

Elf3D original source code is licensed under the MIT License. See `LICENSE`.
Third-party components remain governed by their own licenses and notices, which
are documented separately in `THIRD_PARTY.md`.

## Graphics initialization and lifetime

The host creates and makes an OpenGL 4.1 core context current, then passes a
generic procedure loader to Elf3D:

```cpp
elf3d::EngineConfiguration configuration;
configuration.opengl.load_procedure = load_opengl_procedure;

auto engine_result = elf3d::Engine::create(configuration);
```

The viewer adapts `glfwGetProcAddress`; GLFW types do not cross the Elf3D API.
Scene mutation and rendering are single-threaded. Viewport creation, resize,
render, native texture access, and destruction occur
on the engine's owning graphics thread with a compatible context current.
The host owns Scenes and Viewports; the Engine must outlive all objects created
from it. Viewports and the engine must be destroyed before the host destroys the
graphics context.

Elf3D uses a right-handed world, column-major matrices, column vectors,
`matrix * vector` transformation, `parent_world * local` composition, and
radians for public angles unless an API explicitly says otherwise. One Elf3D
world-space unit is one meter; distance measurement results use meters as their
canonical value.

## Windows prerequisites

- Windows x64.
- Visual Studio 2022 with the **Desktop development with C++** workload.
- CMake 3.25 or newer.
- Git, used by CMake FetchContent.
- A graphics driver supporting an OpenGL 4.1 core-profile context.

## Configure and build

Run from a Visual Studio Developer PowerShell or another terminal where `cmake`
is available:

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
```

For Release:

```powershell
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release
```

Both presets use one generated Visual Studio solution in
`out/build/windows-debug`; build outputs are separated into `bin/<Config>` and
`lib/<Config>`.

## Run the viewer

After a Debug build:

```powershell
.\out\build\windows-debug\bin\Debug\elf3d_viewer.exe
```

The viewer starts with the procedural 24-vertex/36-index cube and renders into
the RGBA8 off-screen viewport texture with a 24-bit depth attachment. Models can
be opened through `File > Open...`, supplied as the first command-line argument,
or dropped onto the GLFW window. `Reload` replaces the current model only after
a complete successful import; `Close Scene` returns to the cube. The `Camera`
menu fits or resets the view, enables or disables navigation, and opens
navigation settings. The `Tools` menu switches between Select (`S`) and Measure
Distance (`M`), and opens the Clipping panel. The `Clipping` menu enables or
flips the section plane, adds boxes from visible bounds, toggles helper
overlays, fits to clipped content, and clears clipping. The `Selection` menu
clears or enables selection, hides, shows, shows all, isolates selected
entities, exits isolation, and opens the Selection panel. The `Measurement` menu
cancels the current in-progress measurement (`Escape`), clears the stored
measurement (`Delete`), and opens the Measurement panel. `Scene Hierarchy`
displays imported entity hierarchy, local and effective visibility, and
isolation status. `Model Information` shows source, scene bounds, import counts,
decoded image memory, texture cache activity, draw calls, rendered triangles,
overlay counts, and clipping broad-phase statistics. Its Rendering section
controls directional-light direction, intensity, and ambient intensity. Loading
is currently synchronous and may temporarily block the viewer UI.

Example command-line loading:

```powershell
.\out\build\windows-debug\bin\Debug\elf3d_viewer.exe C:\models\scene.glb
```

The project-owned `tests/fixtures/textured_pbr.gltf` model is a small visual
validation case with asymmetric color corners, repeated and clamped sampling,
two materials, a metallic surface, and a rough non-metallic surface. It was
generated for Elf3D and carries no external asset license.

## Viewport navigation

The dockable `3D View` uses engine-owned viewport input and navigation state.
Dear ImGui and GLFW input are translated by the viewer before being passed to
Elf3D; the engine does not query either library.

- Left mouse drag beyond the click threshold: orbit around the current pivot.
- Left mouse click below the threshold: select a visible model entity.
- Shift + left mouse drag: pan.
- Middle mouse drag: pan.
- Mouse wheel over the 3D image: dolly in or out.
- `S`: activate Select mode.
- `M`: activate Measure Distance mode.
- `F`: fit the currently visible Viewport content after visibility, isolation, and clipping.
- `Home`: reset to the canonical three-quarter view and fit the currently visible Viewport content.
- `Escape`: cancel an incomplete measurement; otherwise clear the current selection.
- `Delete`: clear the current measurement.

Navigation starts only when the 3D image item is hovered. An active drag keeps
using pointer deltas until the active button is released, even if the pointer
leaves the image. Wheel input is accepted only over the 3D image, so scrolling
other panels does not zoom the camera. Modal dialogs and text input block
keyboard camera shortcuts. Plain left press is held as a pending click until
movement exceeds the selection threshold; the threshold-crossing frame does not
move the camera, so orbit starts without a jump. The Navigation Settings window
exposes orbit, pan, and zoom sensitivities, vertical orbit inversion,
enable/disable state, and read-only pivot, distance, yaw, pitch, and interaction
diagnostics.

## Picking and selection

The dockable `Selection` panel shows whether selection is enabled, the selected
entity name or diagnostic entity ID, mesh ID, primitive and triangle indices,
world hit position and normal, barycentric coordinates, hit distance, highlight
settings, click threshold, and picking cache counters.

Picking uses viewport render-target pixel coordinates with a top-left origin,
positive X to the right, and positive Y down. The viewer maps the displayed
Dear ImGui image rectangle into render-target pixels, so high-DPI framebuffer
scales and docked panel resizing use the same public Elf3D coordinate system.

The current implementation is CPU-based. A pick builds a world-space ray from
the active perspective camera, tests renderable model-entity bounds in a linear
Scene broad phase, transforms the ray into mesh local space, and traverses a
lazy immutable per-mesh BVH. Mesh BVHs are cached by Engine, Scene, and Mesh
identity, shared by Viewports and instances, and released when the owning Scene
is destroyed. A selected entity is highlighted by a viewport render option that
tints all primitives of that entity in the existing material shader path; it
does not add a geometry pass or modify material assets.

This stage supports one selected entity per Viewport. 3D picking selects visible
renderable model entities and records triangle-hit details. Scene Hierarchy row
selection can select transform-only and camera entities without inventing a
triangle hit. Clicking empty background clears selection, Escape clears
selection, and successful Scene replacement begins with no selected entity.
Failed model loading preserves the current Scene and selection. Multi-selection,
focus on selection, transform gizmos, GPU ID-buffer picking, and outline
rendering are not implemented.

Viewport clipping participates in picking and selection. Fully clipped model
bounds are skipped before BVH traversal, and triangle hits are accepted only
when the exact world hit point passes the active clipping filter, so a clipped
nearest triangle does not block selecting farther surviving geometry.

## Distance measurement

The dockable `Measurement` panel controls the point-to-point distance tool.
Activate Measure Distance mode with `M` or `Tools > Measure Distance`, click one
visible surface point, move the pointer over another visible surface to see a
preview marker, line, and distance label, then click a second point to commit
the measurement. `Escape` cancels only an incomplete measurement; `Delete` or
`Clear Measurement` removes the stored measurement. Switching back to Select
mode preserves a completed measurement and restores normal left-click
selection.

Measurement points are created from the existing CPU `PickHit` path and stored
as stable triangle anchors: scene, entity, mesh, primitive, triangle, and
barycentric coordinates. The resolved public snapshot owns current world-space
positions and normals, so entity and ancestor transform changes move the
measurement with the surfaces. Distance is calculated in world space with
meters as the canonical value. The viewer can display meters, centimeters,
millimeters, feet, inches, or an automatic metric unit.

The renderer receives only neutral overlay primitives: one line plus endpoint
markers for a completed measurement, or one marker plus preview line/marker
while awaiting the second point. Overlay depth can be depth-tested or always
visible. Dear ImGui renders the screen-space text label by projecting the
measurement midpoint through the public viewport projection API.

Measurement placement, preview, overlay, and labels respect persistent Scene
visibility, temporary Viewport isolation, and Viewport clipping. Hidden,
isolation-excluded, or clipped geometry cannot create new anchors. Hiding,
isolating, or clipping away an existing anchor keeps the measurement data and
world distance but suppresses the overlay and label until the anchor is visible
again.

This stage supports one point-to-point measurement per Viewport. Angle, area,
polyline, volume, snapping, annotations, serialization, and export are not
implemented.

## Scene hierarchy and visibility

The dockable `Scene Hierarchy` panel uses an immutable public hierarchy snapshot
cached by hierarchy and visibility revisions. It displays every imported entity
in deterministic depth-first pre-order, preserving Scene root and sibling order.
Unnamed entities use stable diagnostic fallback labels in the viewer.

Persistent visibility belongs to `Scene`. Each entity has a local visibility
flag, defaults visible, and effective visibility is inherited from ancestors.
The renderer, picking, visible-bounds queries, and Fit/Reset View ignore
effectively hidden renderable entities. `Hide Selected` changes only the
selected entity's local flag. `Show Selected` shows the selected entity and its
ancestors while preserving descendant local hidden flags. `Show All` restores
all Scene local visibility flags and does not clear selection or isolation.

Temporary isolation belongs to `Viewport`. `Isolate Selected` displays the
selected entity subtree in that Viewport only, intersected with persistent Scene
visibility. `Exit Isolation` clears that Viewport filter without changing Scene
visibility. Hidden entities remain selectable from the hierarchy, but hidden or
isolation-excluded geometry is not pickable and does not render or highlight.

## Section and clipping

Clipping state belongs to each `Viewport`; it is not stored in `Scene`, Entity,
Mesh, Material, or glTF data. Successful Scene replacement clears the current
Viewport clipping volumes because their coordinates are world-space values for
the previous scene. Failed model loading preserves the existing clipping state.

Each Viewport supports one optional arbitrary world-space section plane and up
to three independent world-axis-aligned clipping boxes. The section plane stores
a point, a normal, an enabled flag, and an explicit retained side. Positive side
means `dot(normal, point_to_test - plane_point) >= 0`; flipping the side changes
which half-space is retained without rewriting the public normal. Plane normals
are normalized internally and boundary points are retained.

Enabled clipping boxes retain points inside their world-space minimum/maximum
bounds. Multiple boxes use union semantics: a point may be inside any enabled
box. The section plane and boxes then combine by intersection: a point must pass
the retained plane half-space and the box union. Disabled boxes keep their
parameters in the snapshot but do not affect filtering. Adding a fourth box
returns a structured limit error.

Rendering uses the same neutral clipping filter as CPU systems. The renderer
does entity broad-phase rejection from transformed bounds, submits intersecting
geometry, and performs exact per-fragment clipping in the existing PBR shader.
Picking, selection, measurement placement and overlay visibility, visible-bounds
queries, Fit to Scene, and Reset View all consume that same filter after Scene
visibility and Viewport isolation. Clipping does not modify Scene visibility,
isolation, mesh geometry, material data, measurement anchors, or picking BVHs.

The Clipping panel can enable, disable, edit, flip, reset, and remove volumes.
`Show Clipping Helpers` draws the section plane and clipping-box wireframes
through the existing neutral overlay path. Current boxes are axis aligned only;
rotated boxes, filled section caps, contour extraction, hatching, mesh cutting,
serialization, and transform gizmos are intentionally not implemented.

## Current glTF subset

Elf3D currently imports the default scene, with first-scene and parentless-node
fallbacks; node names and hierarchy; exact matrix or TRS transforms; reusable
meshes; multiple triangle-list primitives per model node; indexed and
non-indexed geometry; unsigned 8-, 16-, and 32-bit indices; accessor offsets,
strides, normalized conversion, and data URIs; external and GLB buffers;
`POSITION`, `NORMAL`, and `TEXCOORD_0`; normalized integer texture coordinates;
generated normals when `NORMAL` is absent; local and transformed world bounds;
`baseColorFactor`, base-color textures, metallic and roughness factors,
metallic-roughness textures, glTF wrap/filter sampler modes, generated mipmaps,
and `doubleSided`. Images may be external PNG/JPEG files, PNG/JPEG base64 data
URIs, or PNG/JPEG GLB buffer views. One decoded image is reused by all referring
textures and may be uploaded separately for sRGB color and linear numeric roles.
A primitive with no material uses one shared opaque white material. `MASK` and
`BLEND` materials are imported as fully opaque and produce one load-time warning
per material.

Decoded images are tightly packed RGBA8 with top-to-bottom source rows. Pixels
and glTF UVs are uploaded unchanged: OpenGL's v=0 samples the first uploaded row,
so the material path applies no vertical flip. The independent Dear ImGui
framebuffer presentation remains vertically flipped. Base-color textures use
`GL_SRGB8_ALPHA8`; metallic-roughness textures use linear `GL_RGBA8`. Lighting is
computed in linear space and the PBR shader manually encodes once to sRGB for the
non-sRGB RGBA8 off-screen target. Selection highlight tint is mixed into the
linear shaded color before that encode.

Source files are limited to 512 MiB, individual external buffers to 1 GiB,
encoded images to 64 MiB, image dimensions to 16384 pixels per axis, individual
decoded images to 256 MiB, and total decoded image storage per imported Scene to
512 MiB. Node, mesh, primitive, accessor, vertex, and index counts have documented
internal desktop-oriented limits. Loading malformed or unsupported data returns
a structured error without modifying the caller's current scene.

## Current limitations

Meshes are immutable indexed triangle lists with position, normal, and one UV
set. The renderer implements one directional light with Lambert/GGX metallic-
roughness shading, but no IBL, environment maps, shadows, normal maps, occlusion,
emissive lighting, texture transforms, or additional UV sets. Alpha masking and
blending, animation, skins, morph targets, Draco, meshopt, KTX2, glTF cameras/
lights, scene-editing tools, hierarchy editing, and transform tools are not
implemented. Scene Hierarchy does not support renaming, deletion, reparenting,
drag-and-drop, multi-selection, component editing, undo/redo, render layers, or
visibility animation. Unknown required extensions fail loading; optional
extensions are not interpreted. This is intentionally not complete glTF 2.0
material support. Navigation is limited to mouse orbit, pan, wheel dolly,
visible-content fit, and canonical reset; it does not include object focus,
first-person movement, touch, or gamepad input. 3D picking uses a linear visible
Scene broad phase plus per-mesh BVHs, not a Scene-wide acceleration structure.
