# Changelog

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
