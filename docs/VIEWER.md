# Elf3D Reference Viewer

This document is the complete command and control reference for the desktop
viewer. For a task-oriented walkthrough, see `GUIDE.md`.

## Opening Models

The viewer accepts `.gltf` and `.glb` files through:

- **File > Open...**;
- the Open toolbar button;
- drag and drop;
- the first command-line argument.

**Reload** opens the current file again. **Close Scene** returns to an empty
scene. A failed load leaves the current scene open and displays an error.
The toolbar file group contains Open followed by Save As; Reload remains in the
File menu.

At launch, the first interface frame uses an empty viewport without creating
the main PBR pipeline or studio environment. A model passed on the command line
is opened only after that first interface frame.

Double-clicking a file in Save As chooses it as the save target and retains the
replacement confirmation. Right-click a file in either model dialog to open it
in Elf3D, copy its quoted full path, inspect its properties, or send it to an
installed EmEditor, Notepad, or Notepad++. Missing editors are shown disabled.

## Workspace

| Area | Purpose |
| --- | --- |
| **3D View** | Scene display and pointer navigation |
| **Scene Browser** | Authored objects, mesh resources, selection, and visibility |
| **Model Information** | Scene statistics and load diagnostics |
| **Properties** | Selected object/resource details and highlight settings |
| **Rendering** | Background, studio lighting, display transform, and camera evidence |
| **Measurement** | Distance tool, units, and overlay style |
| **Navigation Settings** | Navigation sensitivity and camera state |
| **Status Bar** | Active tool, viewport, and frame information |

Use **Window** to show or hide panels. **Window > Reset Layout** restores the default
docking arrangement. The 3D viewport tab displays the loaded model filename,
including its extension, and uses **3D View** when the scene is empty.

Docking is saved separately beside the viewer preferences, with a versioned
`.layout` file. Existing supported layouts keep their positions and docking when
panel names migrate. A newer unsupported layout is preserved until explicit
**Reset Layout**; ordinary preference changes do not replace it.

## Rendering Controls

The **Rendering** panel provides VSync, standard PBR or unlit shading, and
100%, 50%, and 25% render scales. VSync, standard PBR, and 100% scale are
the defaults. Idle 3D content reuses its last resolved texture while event
processing, the interface, and window presentation continue normally.
**Model Information** shows current geometry/material/image statistics and
renderer resource counters.

The viewport clear color starts at RGBA `(213, 227, 240, 255)`.

The Lighting subsection starts with the energy-calibrated high-contrast studio
profile: directional intensity `2.0`, environment intensity `2.0`, environment
rotation `0 degrees`, exposure `0 EV`, Standard tone mapping, and legacy
ambient `0.0`. Environment intensity is the normal fill/reflection control. Rotating
the environment moves material highlights without changing geometry. **PBR
Neutral** remains available for Khronos reference comparisons, **None
(diagnostic)** disables tone mapping, and **Reset Lighting** restores the exact
profile above. These controls are intentionally not saved in preferences; each
launch starts from the standard profile.

**Camera Evidence** displays and copies the camera local matrix, perspective
field of view and depth range, viewport extent, and all current lighting,
environment, and display-transform settings for repeatable comparisons.

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

Forward and backward movement with `W`/`S` uses twice the previous keyboard
dolly rate. Sideways movement with `A`/`D` and vertical movement with `Q`/`E`
use half the previous keyboard pan rate.

During a drag the pointer is captured, so movement can continue beyond the edge
of the 3D panel. Open **Camera > Navigation Settings...** to change orbit, pan,
zoom, and vertical-orbit behavior.

## Selection and Visibility

Activate selection with `S`, **Tools > Select**, or the Select toolbar button.

| Input or command | Action |
| --- | --- |
| `Ctrl` + left click | Replace selection with visible geometry |
| `Ctrl` + `Shift` + left click | Toggle visible geometry in the selection |
| Browser row click | Replace selection with the authored object or resource |
| `Ctrl` + browser row click | Toggle membership |
| `Shift` + browser row click | Select a range in the current visible list |
| **Select All** / **Clear Selection** | Select the current hierarchy / clear selection |
| `Escape` | Clear selection |
| `Shift` + left click | Hide visible geometry |
| **Hide Selected** | Hide all selected entities |
| **Show Selected** | Show selected entities and required ancestors |
| **Show All** | Restore scene visibility and exit isolation |
| **Isolate Selected** | Display the union of selected hierarchy branches |
| **Exit Isolation** | Return to normal scene visibility |

**Objects** shows authored scenes, detached forests under **Outside Scenes**,
and **Runtime Objects** such as the application camera. **Meshes** shows the
retained Document mesh table and its primitives, including unused resources.
The viewer retains its existing default import policy, so this tab covers the
supported resources retained by that policy. A Scene without a Document keeps
its runtime tree and explains why resource data is unavailable.

Search keeps matching ancestors. Type filters, Expand/Collapse, keyboard
navigation and explicit reveal between object/resource details are available.
Names are compact, with ellipsis and complete tooltips; names never identify an
object. Resource selection highlights its active primitive instances while
remaining separate from object selection. Resource context actions explicitly
frame, hide, show or isolate all active instances of the clicked row in the
current view. Unused resources remain inspectable without spatial actions.
Switching tabs preserves the semantic selection domain.

Selection stores validated typed IDs. The primary active selected object receives
the viewport highlight; the tree shows the entire selection. Isolation is
independent of object visibility, and **Show All** also exits isolation and
clears view-local primitive masks.
Selection and view hiding do not remove geometry from **Save As** output.

## Measurement

Activate the distance tool with `M`, **Tools > Measure Distance**, or the
Measurement panel.

1. Click the first visible surface.
2. Move the pointer to preview the result.
3. Click the second visible surface.

Use `Escape` to cancel an incomplete measurement and `Delete` to clear the
measurement. The panel provides automatic metric, meter, centimeter,
millimeter, foot, and inch display modes plus overlay style controls.

## Menus

- **File**: Open, Save, Save As, Reload, Close Scene, Exit.
- **View**: Standard PBR, Unlit and camera/navigation controls.
- **Window**: panels, status bar, layout reset, theme, UI scale and fonts.
- **Tools**: Select, Measure Distance and measurement actions/settings.
- **Selection**: select all, clear, hide, show, isolate, show all, settings.
- **Help**: About Elf3D.

Keyboard shortcuts are suspended while a modal dialog or text editor has input
focus.

**Open** presents the local glTF asset browser with favorites, locations,
storage, recent folders, path and search controls, and an explicit selected-file
area. **About Elf3D** shows the viewer version, supported model and graphics
formats, platform, license, and expandable build details.
The last folder of a successfully opened or saved model is remembered in the
current user's application settings and restored on the next launch.

Window > Theme switches between Light and Dark. UI Scale adjusts interface
spacing and fonts together; Fonts > Text Size adjusts text independently. These
settings are remembered without resetting your docking layout or changing the
model. Light and 100% are the defaults. Repeated scale changes derive from the
original style, rather than accumulating rounding changes.

Backface Culling is a per-Viewport view override, available through the View
menu and Rendering panel. It defaults to respecting authored material sidedness.
Disabling it displays and picks both faces, including navigation focus depth,
without editing materials or changing normal Save output.
