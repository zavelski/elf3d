# Elf3D 0.6.0 Known Limitations

Purpose: Record supported boundaries and known limitations for Elf3D 0.6.0.

Applicable version: 0.6.0

Document status: Local release snapshot.

Release source identifier: local tag `v0.6.0` after release commit.

Implementation source paths: `README.md`, `include/elf3d`, `modules`,
`integrations/imgui`, `apps/viewer`, `scripts/package_release.ps1`

Known limitations: This document is the limitation record.

Related documents: `RELEASE_CHECKLIST.md`, `VALIDATION_SUMMARY.md`,
`RELEASE_ARTIFACTS.md`

## Platform And Packaging

- Windows x64 with Visual Studio 2022 and OpenGL 4.1 core profile is the only
  locally validated platform/backend.
- Linux and macOS are not validated for 0.6.0.
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
- No animation, skins, morph targets, cameras, lights, tangent-space normal
  maps, occlusion maps, emissive maps, compression extensions, KTX2/BasisU,
  material variants, transmission, clearcoat, sheen, volume, shadows,
  image-based lighting, post-processing, or multiple graphics backends are
  implemented.
- Transparent alpha sorting and order-independent transparency are not
  implemented.

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

- Complete manual viewer interaction and visual validation is not automated.
- GitHub CI, tag workflow, published assets, and public clone can be verified
  only after publication.
- No external model corpus or performance benchmark metrics were run.
