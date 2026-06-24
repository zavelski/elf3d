# Changelog

All notable project changes are recorded here when they are relevant to a
release baseline.

## 0.1.0 - Public Release Baseline

Elf3D 0.1.0 is the first audited public visualization baseline.

### Implemented

- Public `elf3d` shared-library facade with `Engine`, `Scene`, `Viewport`,
  hierarchy snapshots, version functions, `Result<T>`, and project-owned value
  types.
- Internal static modules for core, math, assets, image decoding, scene,
  glTF import, graphics abstraction, OpenGL backend, renderer, viewport,
  interaction, navigation, picking, selection, visibility, measurement, and
  clipping.
- Optional `elf3d_imgui` integration target and `elf3d_viewer` reference
  application.
- Static glTF/GLB importer for bounded triangle geometry, scene hierarchy,
  TRS and matrix transforms, external/data/GLB buffers, PNG/JPEG images,
  textures, samplers, and opaque metallic-roughness material factors.
- OpenGL 4.1 off-screen viewport renderer with opaque PBR-style shading,
  GPU mesh and texture caches, selection highlighting, clipping, and tool
  overlays.
- CPU picking with per-mesh BVH cache, viewport selection, persistent scene
  visibility, viewport isolation, one distance measurement, one section plane,
  and up to three axis-aligned clipping boxes per viewport.
- Scene cache-release lifetime remediation: scene destruction no longer keeps
  a raw `Engine::Impl*` callback context.

### Documentation

- Added verified technical documents under `docs/`.
- Added the living `PROJECT_STATE_EN.md` baseline.
- Added audit, validation, remediation, and release-readiness records.
- Added documentation maintenance policy and update checklist.
- Added public repository documentation, contribution guidance, security
  reporting guidance, issue templates, and pull-request template.
- Added the MIT project license while preserving separate third-party notices.

### Validation

- Debug configure/build passed with Visual Studio bundled CMake.
- Debug CTest passed 16/16.
- Release configure/build passed with Visual Studio bundled CMake.
- Release CTest passed 16/16.
- Public headers compiled individually as forced includes with MSVC C++20,
  `/permissive-`, `/W4`, and `/WX`.
- Debug and Release `elf3d_viewer` processes launched with
  `tests/fixtures/textured_pbr.gltf` and exited cleanly through the window
  close path.
- The packaged Windows Release viewer was manually validated by the user for
  navigation, picking, selection, hierarchy synchronization, visibility,
  isolation, measurement, clipping, reload, close-scene, failed-load
  preservation, and normal shutdown.

### Known Limitations

- Manual packaged viewer interaction validation was user-performed for the
  0.1.0 publication baseline; it is not automated.
- Remote GitHub CI, GitHub Release verification, public clone validation,
  external model corpus testing, and performance measurements are not recorded
  in this changelog entry.
- The public DLL surface is a C++ API requiring compatible compiler, standard
  library, and CRT settings. There is no stable C ABI in 0.1.0.
- Import warnings are written to `std::clog`; the public scene-load result does
  not expose warnings programmatically.
- Rendering is opaque-only. glTF `baseColorFactor` alpha, alpha mask, and alpha
  blend are not rendered.
- Animations, skins, morph targets, cameras, lights, normal maps, occlusion,
  emissive maps, mesh compression, KTX2, and runtime plugins are unsupported.
- OpenGL resource destruction requires the host to keep a compatible context
  current on the owning graphics thread during viewport and engine shutdown.

### Compatibility Notes

- Built and tested on Windows x64 with Visual Studio 2022, MSVC 19.44, Windows
  SDK 10.0.26100.0, and CMake/CTest 3.31.6 from Visual Studio bundled tools.
- The engine must not be treated as toolchain-independent binary ABI in this
  release candidate.
