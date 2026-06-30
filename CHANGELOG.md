# Changelog

All notable project changes are recorded here when they are relevant to a
release baseline.

## 0.7.2 - Alpha Channel And Release Process Hardening

Elf3D 0.7.2 follows the published 0.7.1 baseline with a focused correctness and
release-process pass from the post-release audit.

### Implemented

- Corrected OpenGL source-over alpha blending for material and overlay draws by
  using a separate alpha blend equation. The resolved viewport alpha now remains
  opaque over an opaque clear instead of being attenuated by source alpha twice.
- Extended the hidden-context OpenGL smoke test to verify the resolved viewport
  texture extent and alpha channel in addition to the linear-blend RGB pixel.
- Released display-resolve shader/VAO resources immediately when a render
  target is resized to zero.
- Made `scripts/package_release.ps1` derive the default package version from
  `CMakeLists.txt`.
- Replaced `Compress-Archive` packaging with a sorted `ZipArchive` writer using
  fixed entry timestamps. This makes the ZIP deterministic for a fixed staged
  file set; MSVC binary reproducibility is still not claimed.
- Updated the GitHub release workflow to derive the version from CMake, validate
  it against the tag, and reuse that version for package paths, artifact names,
  release title, and release notes.
- Refreshed living documentation and 0.7.2 release records to reflect the
  published 0.7.1 baseline and the 0.7.2 audit fixes.

### Compatibility

- No third-party dependency revisions changed.
- Public API entry points remain source-compatible. The public C++ ABI remains
  matched-toolchain and consumers should rebuild against the 0.7.2 headers.

## 0.7.1 - Renderer Correctness And Release Baseline

Elf3D 0.7.1 prepares a local release baseline on top of the existing static
glTF/GLB compatibility work and stabilizes important reference-viewer viewport
interaction behavior.

### Implemented

- Kept the 0.6.0 glTF compatibility baseline in the 0.7.1 release state:
  UV0/UV1 storage, per-slot `texCoord`, `KHR_texture_transform`, structured
  load diagnostics, material fallbacks, strip/fan conversion, and the local
  corpus probe remain validated scope.
- Fixed the About dialog first-open placement by centering it with Dear ImGui's
  next-window positioning before the first rendered frame.
- Fixed mouse-wheel dolly so hover over the 3D view is sufficient even when the
  docked 3D view no longer has keyboard/window focus.
- Prevented a quick click from leaving an off-axis examine pivot that later
  causes wheel zoom to rotate or jump the camera.
- Moved material shader output back to linear color and added an OpenGL display
  resolve pass so alpha blending occurs before sRGB transfer encoding.
- Added an automated hidden-context OpenGL smoke test that compiles the real
  GLSL path, renders overlapping transparent geometry, and checks the resulting
  sRGB pixel.
- Tightened glTF resource limits so triangle strip/fan expansion is counted
  against the final imported triangle-list index limit before buffers are
  loaded.
- Kept `Engine::load_scene()` free of host-visible logging; structured import
  diagnostics remain available through `Engine::load_scene_with_report()`.
- Corrected public loader snippets to return `elf3d::GraphicsProcedure`.
- Normalized current technical document verification fields to
  `Last verified Git commit`.
- Added focused navigation regression tests for hover-wheel-without-focus and
  wheel zoom after a click-derived pivot.
- Updated runtime version data, public API version tests, release packaging,
  workflow metadata, living documentation, and local release records to
  `0.7.1`.

### Compatibility

- No third-party dependency revisions changed.
- Public API entry points remain source-compatible. The public C++ ABI remains
  matched-toolchain and consumers should rebuild against the 0.7.1 headers.
- GitHub publication, release asset upload, asset checksum verification, and
  public clone verification were completed after the source release commit and
  recorded in `docs/releases/0.7.1/PUBLICATION_REPORT.md`.

## 0.6.0 - Self-contained Dependencies And glTF Compatibility Baseline

Elf3D 0.6.0 makes the repository self-contained for the currently used
third-party source dependencies and records the current glTF compatibility
baseline after the 0.4.0 module migration.

### Implemented

- Vendored the used source subsets for Dear ImGui, GLFW, GLM, GLAD, cgltf, and
  stb under `third_party/` as ordinary tracked files.
- Removed the normal configure/build dependency on `FetchContent`, network
  downloads, external clones, and `_deps/*-src` source trees.
- Kept third-party license notices with each vendored dependency and preserved
  release-package copies under `third_party_licenses/`.
- Updated dependency CMake wiring so the public `elf3d` DLL is still assembled
  from internal OBJECT-library modules while dependency source is resolved only
  from the local repository.
- Recorded the current static glTF/GLB support scope, including UV0/UV1 storage,
  material texture-coordinate selection, texture transforms, alpha values,
  sampler handling, generated normals, scene-load reporting, and a corpus probe
  test target.
- Moved local build/output expectations under repository-owned ignored build
  locations and kept generated Visual Studio projects outside tracked source.
- Updated runtime version data, public/module smoke version tests, release
  packaging, workflow metadata, and living documentation to `0.6.0`.

### Compatibility

- Public API remains C++ and toolchain-matched; consumers must rebuild against
  the 0.6.0 headers and compatible MSVC runtime.
- The public API still does not expose Dear ImGui, GLFW, native OpenGL/GLAD,
  GLM, cgltf, or private module types.
- GitHub publication, release asset upload, and public clone verification are
  intentionally left for a later manual step.

## 0.4.0 - Complete Named-Module Migration

Elf3D 0.4.0 completes the internal C++20 named-module migration while
preserving the public C++ DLL API and the single `elf3d` shared-library
product.

### Implemented

- Added real named-module interface and implementation units for all 18
  internal engine OBJECT-library boundaries.
- Kept exported module interfaces project-owned and GLM-neutral; GLM and other
  third-party types remain implementation details.
- Replaced internal compatibility-header includes with direct `import elf.*;`
  declarations in the facade, engine code, and module tests.
- Removed all 22 obsolete import-only shim headers and their CMake source-list
  entries after confirming that none belonged to the public or install SDK
  surface.
- Added a module import smoke test covering every internal named module.
- Updated local release packaging and the tag-triggered GitHub workflow for
  `0.4.0` Windows viewer artifacts.

### Compatibility

- Public headers under `include/elf3d` and the public DLL API are unchanged by
  the shim removal.
- Compared with the 0.2.0 public baseline, `OrbitNavigationSettings` includes
  `invert_vertical_orbit`; C++ consumers must rebuild against the 0.4.0 headers
  and compatible runtime.
- Named-module artifacts remain internal build products and are not
  distributed as an SDK.

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
- Converted internal engine build groups from static libraries to CMake OBJECT
  libraries linked into the single public `elf3d` DLL.
- Added project-owned C++20 named-module interfaces for each internal engine
  OBJECT library, kept DLL symbol export explicit, and added an import smoke
  test for the module set.
- Updated build and service guidance for Visual Studio 2022 v17.14.35, CMake
  `FILE_SET CXX_MODULES`, and generated module artifact handling.
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
