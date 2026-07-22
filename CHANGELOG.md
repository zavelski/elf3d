# Changelog

## 0.8.9

- Raised the bounded glTF hierarchy limit to 8,192 levels and extended the
  iterative runtime-scene regression to cover deeply nested BIM exports.
- Raised source and embedded-BIN limits to 3 GiB and added a guarded
  compatibility repair for signed 32-bit buffer-field overflow in large GLBs.
- Made imported glTF textures without an explicit minification filter use a
  complete trilinear mip chain, while preserving author-specified filters and
  accounting for every generated mip level in texture-residency statistics.
- Raised the total decoded-image budget to 2 GiB for 64-bit builds while
  retaining the 512 MiB bound for 32-bit builds.

## 0.8.7

- Reduced large-scene CPU submission cost through pass-scoped OpenGL state,
  indexed renderer caches, reusable draw packets, cached scene spatial data,
  and camera-frustum culling.
- Added compact static vertex layouts that retain source attributes while
  avoiding unused UV and color storage for position/normal-only geometry.
- Added nonblocking CPU/GPU rendering and picking diagnostics, a repeatable
  hidden-context benchmark, configurable VSync, diagnostic shading and render
  scale, and retained idle viewport frames.
- Stabilized orbit-entry and picking work with separate focus-depth and ID
  targets, bounded request arbitration, floating-point input processing, and
  deterministic frame-rate, wheel, and DPI regression coverage.

## 0.8.6

- Made `Engine::load_scene()` the single scene-loading operation, returning the
  loaded Scene together with structured compatibility diagnostics and
  preserving metadata warnings through the shared-library facade.
- Hardened Engine- and Document-scoped identities with non-address owner
  tokens and distinct machine-readable failure codes for every persistent
  Document ID family.
- Simplified the model construction and mutation surface by keeping importer
  staging private and making validated `replace_primitive()` the canonical
  geometry replacement operation.
- Tightened rendering, math, picking, tool, and test boundaries by introducing
  focused render vocabulary, containing GLM in the math boundary, removing
  shared mutable test state, and consolidating repeated bounds and framebuffer
  operations.

## 0.8.5

- Standardized public engine operation names around explicit entity creation,
  camera descriptions, local visibility, render statistics, and loaded
  Document export semantics.
- Unified identical model and runtime POD vocabulary in `model_types.h`,
  removing redundant `Model*` type families while keeping document-scoped DTOs
  distinct.
- Added six compile-checked integration examples covering embedded rendering,
  load diagnostics, procedural scenes, picking and selection, Document
  round-trips, and multiple viewports.
- Added precise `entity_has_no_camera` failures across camera-dependent Scene
  and Viewport operations and strengthened public API, module-graph, and policy
  enforcement coverage.

## 0.8.4

- Reworked the public README into a product overview with quick links, feature
  highlights, product entry points, build instructions, current scope, and a
  repository map.
- Added a Mermaid composition diagram that presents the four public products,
  nine grouped CMake module targets, and their dependency direction.
- Added a credited Elf3D Viewer screenshot of the Madame Walker Theatre model
  plus original social-preview artwork, with explicit CC BY 4.0 attribution
  and third-party notice coverage.

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
