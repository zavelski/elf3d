# Elf3D 0.2.0 Known Limitations

Purpose: Record supported boundaries and known limitations for Elf3D 0.2.0.

Applicable version: 0.2.0

Document status: Release snapshot.

Last verified implementation commit: pending 0.2.0 release source commit

Implementation source paths: `README.md`, `include/elf3d`, `modules`,
`integrations/imgui`, `apps/viewer`, `scripts/package_release.ps1`

Known limitations: This document is the limitation record.

Related documents: `RELEASE_CHECKLIST.md`, `VALIDATION_SUMMARY.md`,
`RELEASE_ARTIFACTS.md`

## Platform And Packaging

- Windows x64 with Visual Studio 2022 and OpenGL 4.1 core profile is the only
  validated platform/backend.
- Linux and macOS are not validated for 0.2.0.
- SDK packaging is deferred; only the Windows viewer ZIP is published.
- The package expects `assets/font` and `assets/icon` beside
  `elf3d_viewer.exe`.

## API And ABI

- The public DLL surface is a C++ API requiring compatible compiler, standard
  library, and dynamic MSVC runtime settings.
- There is no stable C ABI.
- There is no runtime plugin ABI.
- Import warnings are written to `std::clog`; they are not returned through a
  public load report.

## Rendering And Assets

- Rendering is opaque-only.
- glTF `baseColorFactor` alpha, alpha mask, and alpha blend are not rendered.
- No shadows, image-based lighting, post-processing, or multiple graphics
  backends are implemented.
- No animation, skins, morph targets, cameras, lights, normal maps, occlusion,
  emissive maps, compression extensions, KTX2, texture transforms, or
  additional UV sets are supported.

## Interaction And Tools

- One entity can be selected per viewport.
- One distance measurement can be stored per viewport.
- Clipping boxes are world-axis-aligned.
- No transform gizmos, undo/redo, multi-selection, annotations, or scene editing
  tools are implemented.

## Lifetime And Threading

- Scene mutation, navigation, picking, rendering, and resource management are
  single-threaded.
- Viewport creation, rendering, native texture access, and destruction require a
  compatible current OpenGL context on the owning graphics thread.
- Incorrect OpenGL shutdown order can leak GPU resources because invalid GL
  deletes are skipped.

## Validation Gaps

- No external model corpus was run.
- No performance benchmark metrics are claimed.
