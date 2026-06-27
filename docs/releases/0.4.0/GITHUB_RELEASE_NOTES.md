# Elf3D 0.4.0

Purpose: Provide release notes for a future Elf3D 0.4.0 GitHub Release.

Applicable version: 0.4.0

Document status: Draft release notes pending publication gates.

Last verified implementation commit: pending 0.4.0 release source commit

Implementation source paths: `CHANGELOG.md`, `README.md`, `include/elf3d`,
`modules`, `apps/viewer`, `scripts/package_release.ps1`

Known limitations: These notes describe a local release candidate. No tag,
GitHub Release, published asset, or public clone has been verified.

Related documents: `RELEASE_CHECKLIST.md`, `VALIDATION_SUMMARY.md`,
`KNOWN_LIMITATIONS.md`, `RELEASE_ARTIFACTS.md`

Elf3D 0.4.0 completes the internal C++20 named-module migration and includes
the viewer workflow and Windows startup refinements prepared after 0.2.0. The
public product remains one `elf3d` shared library plus the reference viewer.

## Supported Platform

- Windows x64.
- Visual Studio 2022 v17.14.35 or newer.
- OpenGL 4.1 core-profile graphics driver.

Linux and macOS are not validated for this release.

## Included Download

- `elf3d-viewer-0.4.0-windows-x64.zip`
- `SHA256SUMS.txt`

SDK packaging remains deferred until install/export rules and an external
consumer validation workflow exist.

## Highlights

- Real C++20 named-module interfaces and implementation units for all 18
  internal engine OBJECT-library boundaries.
- Direct internal module imports with all 22 transitional import-only shim
  headers removed after public/install-surface review.
- GLM-neutral exported module boundaries while preserving the existing public
  C++ DLL API under `include/elf3d`.
- Module import smoke coverage for every internal named module.
- Blender-like Open File dialog with navigation, search, sidebar locations,
  file metadata, and Open/Cancel actions.
- Refined dock-panel title sizing and compact per-tab close controls.
- Orbit drag follows pointer movement while preserving the vertical inversion
  setting.
- `OrbitNavigationSettings::invert_vertical_orbit` exposes that vertical
  direction choice to public C++ hosts; consumers should rebuild for 0.4.0.
- Windows GUI-subsystem startup without an intermediate console window.
- Existing static glTF/GLB import, OpenGL off-screen rendering, GPU-first
  picking, navigation, selection, visibility, measurement, and clipping tools.

## Known Limitations

- Windows/OpenGL is the only validated platform/backend.
- The public DLL surface is a C++ API and not a stable C ABI.
- Named-module BMI/IFC artifacts are internal build products, not SDK assets.
- Rendering is opaque-only; alpha mask and alpha blend are not rendered.
- No animation, skins, morph targets, compression extensions, KTX2, cameras,
  lights, normal maps, occlusion, emissive maps, shadows, or image-based
  lighting.
- Scene mutation and rendering are single-threaded.
- OpenGL resources require a compatible context during shutdown.

## Documentation

- `README.md`
- `docs/MODULE_MAP.md`
- `docs/PUBLIC_API_OVERVIEW.md`
- `docs/GLTF_SUPPORT.md`
- `docs/RENDERING_PIPELINE.md`
- `docs/VIEWPORT_AND_TOOLS.md`
- `docs/LIFETIME_AND_THREADING.md`
- `docs/TESTING.md`
- `docs/releases/0.4.0/`

## License

Elf3D original source code is licensed under the MIT License. Third-party
components remain governed by their own notices in `THIRD_PARTY.md` and
`third_party/licenses/`.
