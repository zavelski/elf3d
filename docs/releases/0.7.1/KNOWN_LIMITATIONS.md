# Elf3D 0.7.1 Known Limitations

Purpose: Record supported boundaries and known limitations for Elf3D 0.7.1.

Applicable version: 0.7.1

Document status: Local release snapshot.

Last verified Git commit: local tag `v0.7.1` after release commit

Implementation source paths: `README.md`, `include/elf3d`, `modules`,
`integrations/imgui`, `apps/viewer`, `scripts/package_release.ps1`

Known limitations: This document is the limitation record.

Related documents: `RELEASE_CHECKLIST.md`, `VALIDATION_SUMMARY.md`,
`RELEASE_ARTIFACTS.md`

## Platform And Packaging

- Windows x64 with Visual Studio 2022 and OpenGL 4.1 core profile is the only
  locally validated platform/backend.
- Linux and macOS are not validated for 0.7.1.
- SDK packaging is deferred; only the Windows viewer ZIP is prepared locally.
- Named-module BMI/IFC artifacts are compiler-specific build outputs and are
  not distributable SDK artifacts.
- GitHub publication and public asset verification are not part of this local
  preparation.

## API And ABI

- The public DLL surface is a C++ API requiring compatible compiler, standard
  library, architecture, and dynamic MSVC runtime settings.
- There is no stable C ABI or runtime plugin ABI.
- glTF load warnings are returned through `SceneLoadReport` when using
  `Engine::load_scene_with_report`; hosts that call `Engine::load_scene` receive
  only the loaded scene result.

## Rendering And Assets

- Rendering is still a bounded static visualization path, not full glTF 2.0.
- Normal textures are imported and preserved but are not rendered until a
  complete tangent-space path exists.
- No animation playback, skinning, morph deformation, scene-light model,
  orthographic camera model, compression decoders, KTX2/BasisU decoder,
  material variants, transmission, clearcoat, sheen, volume, shadows,
  image-based lighting, post-processing, or multiple graphics backends are
  implemented.
- Alpha blending is performed in the linear viewport render target before
  display transfer encoding, but it still uses a simple model-origin sort;
  per-triangle sorting and order-independent transparency are not implemented.
- Picking remains geometry-based and does not sample alpha-masked or blended
  texture alpha.

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

- Complete manual viewer interaction and broad reference-image visual
  validation are not automated. The focused `elf3d.opengl_render_smoke` test
  covers one real OpenGL shader/pixel path when a hidden context is available.
- GitHub CI, tag workflow, published assets, and public clone can be verified
  only after publication.
- No external model corpus or performance benchmark metrics were run.
