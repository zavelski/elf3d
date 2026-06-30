# Elf3D 0.7.1 Release Snapshot Project State

Purpose: Snapshot the Elf3D 0.7.1 project state for local release review.

Applicable version: 0.7.1

Document status: Local release snapshot.

Last verified Git commit: local tag `v0.7.1` after release commit

Implementation source paths: `include/elf3d`, `modules`, `facade/elf3d`,
`integrations/imgui`, `apps/viewer`, `tests`, `third_party`, `CMakeLists.txt`,
`CMakePresets.json`, `.github/workflows`, `scripts`, `docs`

Known limitations: See `KNOWN_LIMITATIONS.md`.

Related documents: `VALIDATION_SUMMARY.md`, `RELEASE_CHECKLIST.md`,
`RELEASE_ARTIFACTS.md`

## Implemented Release Scope

Elf3D 0.7.1 includes:

- version API returning `0.7.1`;
- public `Engine`, `Scene`, `Viewport`, `SceneLoadReport`, and
  `SceneHierarchySnapshot` facades;
- one public `elf3d.dll` assembled from internal CMake OBJECT-library modules;
- current C++20 named-module interfaces and implementation units for internal
  engine module boundaries;
- self-contained vendored source subsets for Dear ImGui, GLFW, GLM, GLAD,
  cgltf, and stb under `third_party/`;
- local dependency CMake wiring that does not use FetchContent, network
  downloads, external clones, or `_deps/*-src` dependency source trees during a
  normal configure/build;
- static glTF/GLB import for bounded triangle geometry with UV0/UV1, vertex
  color, generated normal, material texture-coordinate, texture-transform,
  alpha-value, sampler, and scene-load-report coverage;
- glTF strip/fan resource-limit validation against expanded imported
  triangle-list index counts;
- OpenGL 4.1 off-screen viewport rendering with linear alpha blending before
  display transfer encoding;
- real hidden-context OpenGL smoke coverage for GLSL compilation and one
  transparent pixel result;
- GPU-first picking with CPU refinement/fallback;
- viewport navigation, selection, visibility, isolation, measurement, section
  plane, and clipping-box tools;
- Dear ImGui/GLFW reference viewer with generated toolbar icons, Droid Sans,
  Blender-like file browsing, compact dock controls, and GUI-only startup;
- first-open About dialog centering, hover-based 3D-view wheel dolly without a
  click-to-refocus requirement, and stable wheel zoom after quick click plus
  cursor movement;
- host-owned scene-load diagnostics with no global warning stream output from
  `Engine::load_scene`;
- focused navigation regression coverage for hover-wheel and click-pivot wheel
  paths;
- release packaging with required viewer assets and third-party notices copied
  from their vendored source locations.

## Architecture Boundary

The engine core does not depend on Dear ImGui, GLFW, Windows WIC, or viewer GUI
code. Internal named modules remain hidden from public consumers, and public
DLL symbols remain controlled by `ELF3D_API`.

## Release Decision

`LOCAL-GO_WITH_LIMITATION - automated source/package validation passed; visible manual viewer, GitHub, and public clone validation remain deferred`

GitHub publication, GitHub CI, GitHub Release creation, release asset upload,
and public clone verification are explicitly outside this local preparation.
