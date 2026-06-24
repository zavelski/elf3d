# User Guide

Purpose: Explain how to use the actual Elf3D 0.2.0 reference viewer.

Applicable version: 0.2.0

Document status: Verified from viewer code and README; release validation is
recorded under `docs/releases/0.2.0/`.

Last verified Git commit: pending 0.2.0 release source commit

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
- use `File > Open...`
- drop a file onto the GLFW window

Loading is synchronous and can temporarily block the viewer UI. Failed loading
keeps the current scene.

## Viewer Style And Assets

The viewer uses a light Low.3D-inspired Dear ImGui style with pale panels, grey
title/tab bars, a white default 3D-view clear background, compact status strip,
and generated PNG toolbar icons. The Debug, Release, and packaged viewer
layouts expect an `assets` directory beside `elf3d_viewer.exe`:

- `assets/font/DroidSans.ttf`
- `assets/icon/*.png`

If `DroidSans.ttf` is missing, the ImGui integration falls back to its default
font. Missing toolbar PNGs leave blank fallback buttons, so release packages
must include the full `assets` directory.

## 3D View

The viewer starts with a procedural cube when no model is loaded. Imported
models render into an off-screen Elf3D viewport texture displayed by Dear ImGui.

Controls:

- left drag beyond click threshold: orbit
- X + left drag: pan
- Z + left drag: dolly
- middle drag: pan
- right drag: pan
- mouse wheel over the 3D view: dolly
- plain surface click: set examine pivot
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
texture and renderer statistics, overlay counts, clipping statistics, and basic
rendering settings.

## Error Messages

Model load failures are shown in a modal with source path, error category, and
message. The viewer also writes some failures to `std::cerr`. Import warnings
from the engine currently go to `std::clog`.

## Shutdown

Close the viewer window or use `File > Exit`. The host viewer owns the OpenGL
context and should destroy viewports and engine resources before destroying the
context.

## Current Limitations

- No full glTF material support.
- No alpha masking or blending.
- No animations, skins, morphs, cameras, lights, compression extensions, or KTX2.
- No scene editing, transform gizmos, undo/redo, multi-selection, or annotations.
- No benchmarks recorded.
