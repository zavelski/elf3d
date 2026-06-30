# User Guide

Purpose: Explain how to use the actual Elf3D 0.7.1 reference viewer.

Applicable version: 0.7.1

Document status: Living guide verified from viewer code and README.

Last verified Git commit: local tag `v0.7.1` after release commit

Implementation source paths: `apps/viewer/src/main.cpp`, `integrations/imgui`,
`include/elf3d`, `tests/fixtures/textured_pbr.gltf`

Known limitations: This guide describes the current reference viewer, not a
full editor.

Related documents: `VIEWPORT_AND_TOOLS.md`, `GLTF_SUPPORT.md`, `TESTING.md`,
`LIFETIME_AND_THREADING.md`

## Launching

After a Debug build:

```powershell
.\out\build\windows-debug\bin\Debug\elf3d_viewer.exe
```

After a Release build:

```powershell
.\out\build\windows-release\bin\Release\elf3d_viewer.exe
```

Open a model at startup:

```powershell
.\out\build\windows-debug\bin\Debug\elf3d_viewer.exe tests\fixtures\textured_pbr.gltf
```

## Opening a Model

Supported file types:

- `.gltf`
- `.glb`

Model loading options:

- pass a path as the first command-line argument
- use `File > Open...` or the toolbar Open button to browse folders, sidebar
  locations, recent folders, and filtered `.gltf` or `.glb` files
- drop a file onto the GLFW window

Loading is synchronous and can temporarily block the viewer UI. Failed loading
keeps the current scene.

Successful loads may contain compatibility diagnostics. Open `Model
Information` to review warnings for unsupported optional extensions or visual
fallbacks. The viewer retains these diagnostics with the loaded scene.

## Viewer Style And Assets

The viewer uses a light Low.3D-inspired Dear ImGui style with pale panels, grey
title/tab bars, a white default 3D-view clear background, compact status strip,
and generated PNG toolbar icons. Toolbar buttons use explicit normal, hover,
active, and disabled visual states so icons remain readable on the pale toolbar.
Menus, toolbar, status bar, modals, and 3D overlays use the normal viewer font;
docked panel titles use a slightly reduced intermediate font; panel content
uses a smaller font.
The default layout opens at 1600 x 900, docks `Model Information` beside
`3D View`, and keeps a right column for scene, selection, rendering,
measurement, clipping, and diagnostic panels. `Model Information` uses a white
panel background.
The Debug, Release, and packaged viewer layouts expect an `assets` directory
beside `elf3d_viewer.exe`:

- `assets/font/DroidSans.ttf`
- `assets/icon/*.png`

If `DroidSans.ttf` is missing, the ImGui integration falls back to its default
font. Missing toolbar PNGs leave blank fallback buttons, so release packages
must include the full `assets` directory.

On Windows, the viewer is linked as a GUI subsystem executable, so launching it
opens only the graphical viewer window.

## 3D View

The viewer starts with a procedural cube when no model is loaded. Imported
models render into an off-screen Elf3D viewport texture displayed by Dear ImGui.
The `3D View` window is reserved for the render texture and overlays; rendering
and demo controls live in the side UI.

Controls:

- left drag beyond click threshold: orbit in the mouse-movement direction
- X + left drag: pan
- Z + left drag: dolly
- middle drag: pan
- right drag: pan
- mouse wheel over the 3D view: dolly; moving the cursor back into the 3D view
  is enough, and a click is not required just to restore wheel zoom
- plain surface click: set examine pivot without making a later wheel zoom jump
  toward a stale off-axis click point
- Ctrl + surface click: select
- Shift + surface click: hide hit entity
- `F`: fit visible content
- `Home`: reset view
- `S`: Select tool
- `M`: Measure Distance tool
- `Escape`: cancel incomplete measurement or clear selection
- `Delete`: clear measurement

## Scene Hierarchy

The Scene Hierarchy panel displays imported entities in deterministic
depth-first order. It shows local visibility, effective visibility, and
isolation state. Rows can select entities, including transform-only and camera
entities.

Visibility commands:

- Hide Selected
- Show Selected
- Show All
- Isolate Selected
- Exit Isolation

## Selection

The Select tool picks visible renderable model entities in the 3D view with
Ctrl + surface click. The Selection panel shows selected entity information,
triangle hit details when available, highlight settings, click threshold, and
picking statistics.

Ctrl + clicking empty background clears selection. Hidden, isolation-excluded,
or clipped geometry is not pickable.

## Measurement

Activate Measure Distance with `M` or `Tools > Measure Distance`. Click a
visible surface point, move over another visible point to preview, then click
again to commit.

The Measurement panel controls:

- display unit
- overlay depth mode
- line and marker colors
- line thickness
- marker radius
- cancel current
- clear measurement

Measurements are stored as surface anchors and update when model transforms
change. Hidden or clipped anchors suppress overlay display but keep the stored
measurement.

## Clipping

The Clipping panel supports:

- one section plane
- up to three axis-aligned clipping boxes
- helper overlays
- reset boxes to visible bounds
- fit to clipped content
- clear clipping

Clipping affects rendering, picking, selection, measurement placement,
measurement overlay visibility, fit, and reset.

## Model Information

The Model Information panel reports scene counts, bounds, decoded image memory,
texture and renderer statistics, overlay counts, and clipping statistics. It
also keeps the source path and format visible for the current procedural or
imported scene.

## Rendering

The Rendering panel contains viewport and scene-display controls:

- clear color
- procedural cube rotation
- procedural cube speed
- reset cube transform
- procedural cube base color
- light direction
- light intensity
- ambient intensity
- reset lighting

Imported materials may use UV0 or UV1 independently per texture, texture
offset/scale/rotation, vertex color, alpha mask, simple alpha blend, emissive,
occlusion, unlit, IOR, and specular factors. Normal textures are retained but
render with vertex normals and a visible diagnostic until tangent-space support
is complete.

## Error Messages

Model load failures are shown in a modal with source path, error category, and
message. The viewer also writes some failures to `std::cerr`. Successful-load
diagnostics are shown in `Model Information` through the structured
`SceneLoadReport` returned by `Engine::load_scene_with_report()`.

The About dialog opens centered in the application viewport on first and later
opens.

## Shutdown

Close the viewer window or use `File > Exit`. The host viewer owns the OpenGL
context and should destroy viewports and engine resources before destroying the
context.

## Current Limitations

- No tangent-space normal mapping or advanced layered/transmissive materials.
- Alpha blending uses a simple model-origin sort, not order-independent transparency.
- No animations, skin deformation, morph deformation, scene lights,
  orthographic-camera import, compression decoders, or KTX2.
- No scene editing, transform gizmos, undo/redo, multi-selection, or annotations.
- No benchmarks recorded.
