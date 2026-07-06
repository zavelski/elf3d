# Elf3D Viewer Practical User Guide

This is the informal, task-oriented guide to the reference viewer. For the
complete command and control listing, see `VIEWER.md`.

## Starting the Viewer

Run the Release build:

```powershell
.\out\build\windows-release\bin\Release\elf3d_viewer.exe
```

Open a model immediately:

```powershell
.\out\build\windows-release\bin\Release\elf3d_viewer.exe "C:\Models\scene.gltf"
```

The viewer accepts `.gltf` and `.glb` files. You can also:

- choose **File > Open...**;
- click the Open toolbar button;
- drag and drop a model onto the window.

Without a model, Elf3D shows its procedural demo cube. Loading is synchronous,
so a large model can temporarily freeze the interface. If loading fails, the
current scene remains open.

If you copy the viewer elsewhere, keep its `assets` directory beside the
executable; it contains the UI font and toolbar icons.

## The Interface at a Glance

The main window is a dockable workspace:

- **3D View** contains the rendered scene and receives mouse navigation;
- **Scene Hierarchy** shows entities, cameras, parent-child relationships, and
  visibility;
- **Model Information** shows geometry, material, image, renderer, and import
  diagnostics;
- **Selection** shows the current selection, hit details, and highlight
  settings;
- **Rendering** controls the background color and lighting;
- **Measurement** controls distance measurement and its overlay;
- **Clipping** controls the section plane and clipping boxes;
- **Navigation Settings** controls navigation sensitivity and shows the current
  camera state;
- **Status Bar** shows the active tool, selection, clipping, viewport, and
  performance information.

Panels can be shown or hidden from **View**. Use **View > Reset Layout** if the
docking arrangement becomes inconvenient.

## Scene Navigation

Keep the pointer over the **3D View**.

| Input | Action |
| --- | --- |
| Left drag | Orbit around the current working center. |
| `Space` + left drag | Look around from the current camera position. |
| `X` + left drag | Pan. |
| `Z` + left drag | Dolly forward or backward. |
| Middle drag | Pan. |
| Right drag | Slower pan. |
| Mouse wheel | Dolly forward or backward. |
| Hold left or right mouse + `W` / `S` | Move toward or away from the working center. |
| Hold left or right mouse + `A` / `D` | Pan left or right. |
| Hold left or right mouse + `Q` / `E` | Move downward or upward on the global Y axis. |
| Plain click on geometry while Select is active | Set that surface point as the navigation anchor. |
| `F` | Fit all currently visible content. |
| `Home` | Reset the camera. |

A useful workflow is:

1. Plain-click the part of the model you want to inspect.
2. Scroll to move toward it.
3. Left-drag to orbit around that point.
4. Use middle-drag or `X` + left-drag for small framing corrections.
5. Press `F` if you get lost.

When no explicit surface anchor exists, starting a new orbit chooses a working
center from the visible scene. Hold `Space` while starting a left-drag orbit to
skip that center update and rotate the camera in place from its current world
position; the mode stays fixed until the drag ends.

During an active drag, Elf3D hides and captures the pointer. You can therefore
continue dragging beyond the edge of the 3D panel. Switching between held mouse
buttons changes navigation mode without releasing the pointer.

A small left movement is treated as a click; movement beyond the click threshold
becomes navigation. The threshold can be changed in the **Selection** panel.

Navigation takes priority once a gesture becomes a drag. This allows you to
orbit and pan even while the measurement tool is active.

### Navigation Settings

Open **Camera > Navigation Settings...** to adjust:

- orbit sensitivity;
- pan sensitivity;
- zoom sensitivity;
- inverted vertical orbit;
- navigation enabled or disabled.

The panel also displays the current camera and interaction state.
**Reset Navigation Settings** restores the defaults.

Keyboard shortcuts are suppressed while typing into a text field or while a
modal dialog is open.

## Selecting Objects

Press `S`, choose **Tools > Select**, or click the Selection toolbar button.

Selection in the 3D view requires:

```text
Ctrl + left click
```

A plain click sets a navigation anchor instead of selecting. This is
intentional.

You can also click an item directly in **Scene Hierarchy**. Hierarchy selection
can select cameras and transform-only entities, while viewport selection only
hits visible rendered geometry.

The **Selection** panel shows:

- selected entity and mesh IDs;
- primitive and triangle indices;
- world-space hit position, normal, and distance;
- highlight color and strength;
- selection diagnostics.

There is only one selection at a time. Press `Escape` to clear it, unless an
incomplete measurement is active.

## Visibility and Isolation

Useful commands are available in the **Selection** menu, toolbar, and Scene
Hierarchy:

- **Hide Selected** persistently hides the selected entity;
- **Show Selected** shows it and any hidden ancestors;
- **Show All** restores persistent visibility for all entities;
- **Isolate Selected** temporarily displays only the selected hierarchy branch;
- **Exit Isolation** returns to ordinary visibility.

You can also:

- `Shift` + left-click a visible object to hide it immediately;
- use each hierarchy row's **Hide/Show** button;
- right-click a hierarchy row for Select, Hide, Show, and Isolate commands.

Isolation is separate from scene visibility. Consequently, **Show All** does
not exit isolation; use **Exit Isolation** as well.

The hierarchy distinguishes:

- `local hidden`: the entity itself was hidden;
- `inherited hidden`: an ancestor is hidden;
- `isolated`: the current isolation root.

## Measuring Distance

Press `M`, choose **Tools > Measure Distance**, or select it in the
**Measurement** panel.

Then:

1. Click a visible surface to place the first point.
2. Move the pointer to preview the second point and distance.
3. Click another visible surface to complete the measurement.
4. Click again to begin a new measurement.

The result is a point-to-point distance in world space. The canonical value is
meters, but the display can use:

- automatic metric units;
- meters;
- centimeters;
- millimeters;
- feet;
- inches.

The Measurement panel also controls line and marker colors, thickness, marker
radius, and whether the overlay is depth-tested or always visible.

Shortcuts:

- `Escape` cancels an incomplete measurement;
- `Delete` clears the measurement.

Switching back to Select also cancels an incomplete first point. A completed
measurement remains visible when changing tools.

Measurement points follow their scene geometry. Hidden, isolated, or clipped
geometry cannot be picked. If an existing anchor becomes hidden or clipped, the
measurement remains stored but its overlay is suppressed.

## Clipping the Scene

Open the **Clipping** panel from the toolbar, **Tools > Clipping**, or
**View > Clipping**.

### Section Plane

Enable **Section Plane**, then configure:

- **Point**: a point through which the plane passes;
- **Normal**: the plane direction;
- **Retained side**: the positive or negative half-space.

The `X`, `Y`, and `Z` buttons align the normal to a world axis. **Center** moves
the plane to the center of the currently visible bounds. **Clear** removes it.

The toolbar's section-plane button enables the plane and initially centers it
on visible content. **Flip Section Side** quickly swaps which side remains
visible.

### Clipping Boxes

Choose **Add Box from Visible Bounds** to create a box covering the current
visible content. You can then edit its minimum and maximum coordinates.

Each box can be enabled, reset to visible bounds, or removed. Up to three boxes
are supported.

Multiple enabled boxes use union semantics: geometry inside any enabled box
remains eligible. The box union and section plane are then combined by
intersection, so geometry must also be on the retained side of the section
plane.

Use:

- **Show helpers** to display plane and box outlines;
- **Fit to Clipped Content** to frame only what remains visible;
- **Clear Boxes** to remove boxes only;
- **Clear Clipping** to remove the plane and all boxes.

Clipping affects rendering, picking, selection, measurement, visible bounds,
Fit, and Reset; it does not merely mask the displayed pixels.

## Rendering and Model Information

The **Rendering** panel controls:

- viewport clear color;
- light direction;
- directional light intensity;
- ambient intensity;
- lighting reset.

When the demo cube is active, it also controls cube rotation, speed, transform
reset, and base color.

The **Model Information** panel is the first place to check when a model looks
incomplete. It reports:

- entity, mesh, material, image, and texture counts;
- decoded image memory;
- primitives, vertices, indices, and triangles;
- model bounds;
- draw calls and GPU texture activity;
- clipping statistics;
- import compatibility diagnostics.

A model can load successfully while still producing compatibility or
resource-limit diagnostics.

## Menu Reference

- **File**: Open, Reload, Close Scene, and Exit.
- **View**: show or hide panels and the status bar, open the ImGui demo, and
  reset the layout.
- **Tools**: Select, Measure Distance, and open Clipping.
- **Clipping**: section plane, side flip, add box, helpers, fit, and clear.
- **Camera**: Fit, Reset, enable navigation, and navigation settings.
- **Selection**: clear, hide, show, isolate, show all, and settings.
- **Measurement**: cancel, clear, and settings.
- **Help**: About Elf3D.

**Close Scene** returns to the procedural cube. **Reload** is available only for
imported models.
