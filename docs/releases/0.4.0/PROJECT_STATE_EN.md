# Elf3D 0.4.0 Release Snapshot Project State

Purpose: Snapshot the Elf3D 0.4.0 project state for release review.

Applicable version: 0.4.0

Document status: Release snapshot.

Last verified implementation commit: pending 0.4.0 release source commit

Implementation source paths: `include/elf3d`, `modules`, `facade/elf3d`,
`integrations/imgui`, `apps/viewer`, `tests`, `CMakeLists.txt`,
`CMakePresets.json`, `.github/workflows`, `scripts`, `docs`

Known limitations: See `KNOWN_LIMITATIONS.md`.

Related documents: `VALIDATION_SUMMARY.md`, `RELEASE_CHECKLIST.md`,
`RELEASE_ARTIFACTS.md`

## Implemented Release Scope

Elf3D 0.4.0 includes:

- version API returning `0.4.0`
- public `Engine`, `Scene`, `Viewport`, and `SceneHierarchySnapshot` facades
- one `elf3d` DLL assembled from 18 internal engine OBJECT libraries
- real named-module interfaces and implementation units for all internal engine
  modules, without transitional import-only shim headers
- static glTF/GLB import for bounded triangle geometry
- OpenGL 4.1 off-screen viewport rendering
- GPU-first picking with CPU refinement/fallback
- viewport navigation, selection, visibility, isolation, measurement, section
  plane, and clipping-box tools
- Dear ImGui/GLFW reference viewer with generated toolbar icons, Droid Sans,
  Blender-like file browsing, compact dock controls, and GUI-only startup
- release packaging with required viewer assets and third-party notices

## Architecture Boundary

The engine core does not depend on Dear ImGui, GLFW, Windows WIC, or viewer GUI
code. Internal named modules remain hidden from public consumers, and public
DLL symbols remain controlled by `ELF3D_API`.

## Release Decision

`NO-GO - publication blocked`

Local automated validation and packaging can be completed before publication.
Complete manual viewer validation, remote CI, tag correctness, GitHub Release
asset verification, and a public clone test remain required publication gates.
