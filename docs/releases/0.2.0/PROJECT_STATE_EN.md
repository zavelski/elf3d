# Elf3D 0.2.0 Release Snapshot Project State

Purpose: Snapshot the verified Elf3D 0.2.0 project state for release review.

Applicable version: 0.2.0

Document status: Release snapshot.

Last verified implementation commit: pending 0.2.0 release source commit

Implementation source paths: `include/elf3d`, `modules`, `facade/elf3d`,
`integrations/imgui`, `apps/viewer`, `tests`, `CMakeLists.txt`,
`CMakePresets.json`, `.github/workflows`, `scripts`, `docs`

Known limitations: See `KNOWN_LIMITATIONS.md`.

Related documents: `VALIDATION_SUMMARY.md`, `RELEASE_CHECKLIST.md`,
`RELEASE_ARTIFACTS.md`

## Implemented Release Scope

Elf3D 0.2.0 includes:

- version API returning `0.2.0`
- public `Engine`, `Scene`, `Viewport`, and `SceneHierarchySnapshot` facades
- static glTF/GLB import for bounded triangle geometry
- OpenGL 4.1 off-screen viewport rendering
- GPU-first picking with CPU refinement/fallback
- viewport navigation, selection, visibility, isolation, measurement, section
  plane, and clipping-box tools
- Dear ImGui/GLFW reference viewer with generated PNG toolbar icons, Droid Sans,
  light Low.3D-inspired styling, compact status bar, and right-side dock layout
- release packaging that includes required viewer assets and third-party notices

## Architecture Boundary

The engine core does not depend on Dear ImGui, GLFW, Windows WIC, or viewer GUI
code. Viewer UI and PNG toolbar decoding stay in `elf3d_viewer`; ImGui context
configuration stays in `elf3d_imgui`.

## Validation Summary

The full validation record is maintained in `VALIDATION_SUMMARY.md`.

## Release Decision

`GO - ready for public release`

Local validation, package inspection, CI, tag workflow, GitHub Release
verification, and public clone testing passed. Final branch synchronization is
recorded in `PUBLICATION_REPORT.md`.
