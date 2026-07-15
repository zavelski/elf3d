# Changelog

## 0.8.3

- Raised the glTF node limit to 131,072 and discarded delayed pointer samples
  outside the active viewport so large-model navigation no longer produces
  out-of-bounds picking errors.
- Made glTF/GLB export choose the smallest exact unsigned index width and emit
  tab-indented, human-readable JSON while preserving supported model values and
  opaque retained metadata.
- Replaced the toolbar Reload action with Save As, reduced Q/W/E/A/S/D
  navigation speed by half, and improved dark-dialog text-cursor contrast.
- Added Save As double-click activation plus shared Open/Save file context
  commands for opening, copying the quoted path, viewing properties, and using
  installed EmEditor, Notepad, or Notepad++ applications.
- Fixed clean parallel Visual Studio builds when grouped module sources share a
  basename by separating their generated module-dependency metadata.

## 0.8.2

- Removed all remaining green-profile metric allowances and reduced reviewed
  boundary allowances to 23 while preserving the existing public contracts.
- Split large model, navigation, renderer, viewport, viewer, OpenGL backend,
  and glTF implementation areas into focused private units with explicit
  ownership and dependency boundaries.
- Expanded focused regression coverage and kept the canonical Document,
  rendering, interaction, picking, tools, and viewer workflows behaviorally
  unchanged through the structural cleanup.

## 0.8.1

- Added a professional glTF Open browser and matching Save As workflow with
  `.glb`/`.gltf` output, replacement confirmation, save diagnostics, and a
  `Ctrl+Shift+S` shortcut.
- Added `Scene::save_model()` to the public engine facade so loaded scenes can
  export their retained canonical Document through the existing transactional
  glTF/GLB writer.
- Remembered the last successfully opened or saved model directory between
  viewer launches and improved modal input isolation, draggable About details,
  and scrollbar visibility.

## 0.8.0

- Added the source-integrated `elf3d_model` static library and canonical
  CPU-side `elf3d::Document` API for model construction, inspection,
  validation, and processing without configuring the engine or viewer.
- Reworked glTF/GLB loading to retain all scenes, authored default-scene
  selection, safe source-image and bounded raw-metadata fidelity, and added
  transactional Document export with structured write diagnostics.
- Made the engine derive runtime Scene state from retained Document data and
  updated renderer, picking, tools, tests, CMake presets, and public
  documentation for the new model-first architecture.

## 0.7.9

- Advanced the internal Essential C++ Lite 1.2 migration with a machine-readable
  green-profile baseline, stricter module checks, and negative enforcement
  fixtures.
- Split large facade, navigation, picking, renderer, scene, and viewport
  implementation slices while preserving the public engine, ImGui integration,
  and viewer product boundaries.
- Tightened public API ownership, result/error, scene loading, viewport, and
  validation documentation around the current same-toolchain C++ SDK contract.

## 0.7.8

- Reorganized the source build around grouped internal CMake module targets so
  generated Visual Studio solutions no longer create one production project per
  named C++ module.
- Preserved the existing public engine API, C++ module names, and CTest names
  while consolidating focused module test executables behind grouped drivers.
- Kept image decoding, glTF import, OpenGL backend, ImGui integration, and the
  viewer in boundary-sensitive build areas.

## 0.7.7

- Added Space + left-drag look-around that rotates the camera from its current
  world position for the duration of the gesture.
- Kept in-place look-around latched until the active drag ends, including when
  Space is released during the drag.
- Routed viewer mouse-wheel navigation directly from GLFW so dolly input is
  not deferred behind queued pointer events after captured navigation.

## 0.7.6

- Improved rendering reliability for valid scenes that use very small nested
  model scales.
- Added a practical viewer guide and a complete controls, menus, measurement,
  clipping, and navigation reference.
- Added a self-contained public documentation set for building, testing, glTF
  compatibility, rendering, and C++ API integration.
- Updated the Windows viewer package to include the user guide and reference.
