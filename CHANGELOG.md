# Changelog

All notable project changes are recorded here when they are relevant to a
release baseline.

## 0.3.0 - Viewer Polish And Windows Startup

Elf3D 0.3.0 focuses on reference-viewer polish and Windows startup behavior.

### Implemented

- Refined the docked panel title font, compacted the per-tab close glyphs while
  leaving the dock-node close-all button unchanged, and centered the About
  window when it appears.
- Reversed orbit drag direction so the visible model follows mouse movement by
  default, while preserving the vertical inversion setting.
- Reworked the Open glTF Model dialog into a Blender-like file browser with
  top navigation, search, sidebar locations, file metadata, and a bottom
  selected-path row with Open/Cancel actions.
- Built `elf3d_viewer` as a Windows GUI subsystem executable with a WinMain
  entry point, matching the Low.3D startup approach and avoiding the extra
  console window.
- Updated runtime version data and the public API version test to `0.3.0`.

## 0.2.0 - Viewer Interaction And Low.3D UI Refresh

Elf3D 0.2.0 extends the viewer/testbed slice with GPU-assisted interaction and
a refreshed Low.3D-inspired Windows viewer interface.

### Implemented

- Added GPU-first viewport picking with CPU triangle confirmation and CPU BVH
  fallback.
- Added scene hierarchy reporting, persistent entity visibility, viewport
  isolation, selection details, measurement anchors, and clipping helpers to the
  public viewer workflow.
- Refreshed the Dear ImGui viewer style with Droid Sans, light pale panels,
  compact status bar styling, a Low.3D-like right-side dock layout, and generated
  original PNG toolbar icons.
- Added packaged viewer assets under `assets/font` and `assets/icon`, plus
  release-package checks that require those assets.
- Added Codex workflow documentation and local release/publish helper skills
  under `.agents/skills`.

### Validation

- Debug and Release configure/build/CTest are release gates for this version.
- The Windows viewer package includes Droid Sans and all generated toolbar
  icons.
- Final validation results are recorded in `docs/releases/0.2.0/` after the
  release gates pass.

### Known Limitations

- Windows/OpenGL remains the only validated platform/backend.
- The public DLL surface remains a C++ API and not a stable C ABI.
- Rendering is opaque-only. glTF alpha mask and alpha blend are not rendered.
- Animations, skins, morph targets, cameras, lights, compression extensions,
  KTX2, runtime plugins, and SDK packaging remain unsupported.

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
