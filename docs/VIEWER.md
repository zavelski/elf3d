# Elf3D Reference Viewer

This document is the complete command and control reference for the desktop
viewer. For a task-oriented walkthrough, see `GUIDE.md`.

## Opening Models

The viewer accepts `.gltf` and `.glb` files through:

- **File > Open...**;
- the Open toolbar button;
- drag and drop;
- the first command-line argument.

**Reload** opens the current file again. **Close Scene** returns to the built-in
demo scene. A failed load leaves the current scene open and displays an error.
The toolbar file group contains Open followed by Save As; Reload remains in the
File menu.

Double-clicking a file in Save As chooses it as the save target and retains the
replacement confirmation. Right-click a file in either model dialog to open it
in Elf3D, copy its quoted full path, inspect its properties, or send it to an
installed EmEditor, Notepad, or Notepad++. Missing editors are shown disabled.

## Workspace

| Area | Purpose |
| --- | --- |
| **3D View** | Scene display and pointer navigation |
| **Scene Hierarchy** | Entity hierarchy, selection, and visibility |
| **Model Information** | Scene statistics and load diagnostics |
| **Selection** | Selected object and highlight settings |
| **Rendering** | Background and lighting controls |
| **Measurement** | Distance tool, units, and overlay style |
| **Clipping** | Section plane and clipping boxes |
| **Navigation Settings** | Navigation sensitivity and camera state |
| **Status Bar** | Active tool, viewport, and frame information |

Use **View** to show or hide panels. **View > Reset Layout** restores the default
docking arrangement.

## Performance Diagnostics

The **Rendering** panel includes a diagnostic section with VSync, standard PBR
or unlit shading, 100%, 50%, and 25% render scales, rolling average, median,
p95, p99, maximum frame time, CPU/GPU phase timings, context details, resource
and residency counters, and CSV capture. VSync, standard PBR, and 100% scale
are the defaults. Idle 3D content reuses its last resolved texture while event
processing, the interface, and window presentation continue normally.

**Navigation Settings** includes the default-on focus-depth orbit anchor. Turn
it off only to compare orbit-entry behavior; ordinary navigation retains the
anchored behavior.

## Navigation Controls

Keep the pointer over **3D View**.

| Input | Action |
| --- | --- |
| Left drag | Orbit |
| `Space` + left drag | Look around from current position |
| `X` + left drag | Pan |
| `Z` + left drag | Dolly |
| Middle drag | Pan |
| Right drag | Slower pan |
| Mouse wheel | Dolly |
| Left or right button + `W` / `S` | Move toward or away from the working center |
| Left or right button + `A` / `D` | Move left or right |
| Left or right button + `Q` / `E` | Move down or up along global Y |
| Plain surface click in Select mode | Set a navigation anchor |
| `F` | Fit visible content |
| `Home` | Reset the camera |

During a drag the pointer is captured, so movement can continue beyond the edge
of the 3D panel. Open **Camera > Navigation Settings...** to change orbit, pan,
zoom, and vertical-orbit behavior.

## Selection and Visibility

Activate selection with `S`, **Tools > Select**, or the Select toolbar button.

| Input or command | Action |
| --- | --- |
| `Ctrl` + left click | Select visible geometry |
| Hierarchy row click | Select an entity |
| `Escape` | Clear selection |
| `Shift` + left click | Hide visible geometry |
| **Hide Selected** | Hide the selected entity |
| **Show Selected** | Show the selected entity and required ancestors |
| **Show All** | Restore scene visibility |
| **Isolate Selected** | Display only the selected hierarchy branch |
| **Exit Isolation** | Return to normal scene visibility |

Isolation is independent of persistent scene visibility. Use **Exit Isolation**
after **Show All** when both states were active.

## Measurement

Activate the distance tool with `M`, **Tools > Measure Distance**, or the
Measurement panel.

1. Click the first visible surface.
2. Move the pointer to preview the result.
3. Click the second visible surface.

Use `Escape` to cancel an incomplete measurement and `Delete` to clear the
measurement. The panel provides automatic metric, meter, centimeter,
millimeter, foot, and inch display modes plus overlay style controls.

## Clipping

Open the **Clipping** panel from the toolbar, **Tools**, or **View**.

The section plane is defined by a point, a normal, and the retained side. Axis
buttons align it to X, Y, or Z; **Center** places it at the center of visible
content; **Flip Section Side** changes the retained half-space.

Up to three enabled axis-aligned boxes can be edited. Content inside any
enabled box is retained, then combined with the section-plane result. Use
**Show helpers**, **Fit to Clipped Content**, **Clear Boxes**, and
**Clear Clipping** as needed.

## Menus

- **File**: Open, Save As, Reload, Close Scene, Exit.
- **View**: panels, status bar, demo window, reset layout.
- **Tools**: Select, Measure Distance, Clipping.
- **Clipping**: section plane, flip, add box, helpers, fit, clear.
- **Camera**: Fit, Reset, navigation enable, navigation settings.
- **Selection**: clear, hide, show, isolate, show all, settings.
- **Measurement**: cancel, clear, settings.
- **Help**: About Elf3D.

Keyboard shortcuts are suspended while a modal dialog or text editor has input
focus.

**Open** presents the local glTF asset browser with favorites, locations,
storage, recent folders, path and search controls, and an explicit selected-file
area. **About Elf3D** shows the viewer version, supported model and graphics
formats, platform, license, and expandable build details.
The last folder of a successfully opened or saved model is remembered in the
current user's application settings and restored on the next launch.
