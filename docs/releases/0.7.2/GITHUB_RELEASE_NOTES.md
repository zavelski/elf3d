# Elf3D 0.7.2

Purpose: Provide release notes for a future Elf3D 0.7.2 GitHub Release.

Applicable version: 0.7.2

Document status: Draft release notes pending publication gates.

Last verified Git commit: local tag `v0.7.2` after release commit

Implementation source paths: `CHANGELOG.md`, `README.md`, `include/elf3d`,
`modules`, `third_party`, `apps/viewer`, `scripts/package_release.ps1`

Known limitations: These notes describe a locally prepared release source. No
GitHub Release, published asset, or public clone has been verified.

Related documents: `RELEASE_CHECKLIST.md`, `VALIDATION_SUMMARY.md`,
`KNOWN_LIMITATIONS.md`, `RELEASE_ARTIFACTS.md`

Elf3D 0.7.2 is a focused follow-up to the published 0.7.1 baseline. It fixes
the viewport texture alpha equation for transparent draws, tightens OpenGL
render-target cleanup, and reduces release-process drift in the packaging and
GitHub Release workflow. The public product remains one `elf3d` shared library
plus the reference viewer.

## Supported Platform

- Windows x64.
- Visual Studio 2022 v17.14.35 or newer.
- OpenGL 4.1 core-profile graphics driver.

Linux and macOS are not validated for this release.

## Included Download

- `elf3d-viewer-0.7.2-windows-x64.zip`
- `SHA256SUMS.txt`

SDK packaging remains deferred until install/export rules and an external
consumer validation workflow exist.

## Highlights

- Transparent material and overlay draws now use separate RGB/alpha blend
  factors. This preserves correct source-over alpha in the viewport texture
  while retaining the 0.7.1 linear RGB composition path.
- The hidden-context OpenGL smoke test now verifies the resolved texture extent
  and alpha channel in addition to the transparent RGB pixel.
- OpenGL display-resolve shader/VAO resources are released immediately when a
  render target is resized to zero.
- The package script can derive its version from `CMakeLists.txt` and writes ZIP
  entries in sorted order with fixed timestamps for deterministic archives when
  the staged files are identical.
- The GitHub Release workflow derives the release version from CMake, validates
  it against the tag, and reuses that version for artifact names, package paths,
  release title, and release notes.
- 0.7.1 remains the published compatibility baseline for the larger static
  glTF/GLB, viewer interaction, and color-space work; 0.7.2 hardens that
  baseline without adding new third-party dependencies.
- Existing C++20 named-module and internal OBJECT-library architecture remains
  intact; the external product remains `elf3d.dll`.
- glTF/GLB coverage includes bounded static geometry, node hierarchy, TRS and
  matrix transforms, external/data/GLB buffers, PNG/JPEG textures, samplers,
  UV0/UV1, vertex color, material texture-coordinate selection,
  `KHR_texture_transform`, base-color alpha values, and scene-load reporting.
- Local validation covers public API versioning, internal module imports, glTF
  importer behavior, renderer material propagation, a real OpenGL shader/pixel
  smoke including alpha, navigation regression coverage, release packaging, and
  archive contents.

## Known Limitations

- Windows/OpenGL is the only locally validated platform/backend.
- The public DLL surface is a C++ API and not a stable C ABI.
- Named-module BMI/IFC artifacts are internal build products, not SDK assets.
- Rendering remains a bounded static visualization path. Normal textures are
  imported but not rendered; animation, skinning, morphing, scene lights,
  orthographic cameras, compression decoders, KTX2/BasisU, layered/transmissive
  materials, and order-independent transparency remain unsupported.
- Scene mutation and rendering are single-threaded.
- OpenGL resources require a compatible context during shutdown.

## Documentation

- `README.md`
- `THIRD_PARTY.md`
- `docs/MODULE_MAP.md`
- `docs/PUBLIC_API_OVERVIEW.md`
- `docs/GLTF_SUPPORT.md`
- `docs/RENDERING_PIPELINE.md`
- `docs/VIEWPORT_AND_TOOLS.md`
- `docs/LIFETIME_AND_THREADING.md`
- `docs/TESTING.md`
- `docs/releases/0.7.2/`

## License

Elf3D original source code is licensed under the MIT License. Third-party
components remain governed by their own notices in `THIRD_PARTY.md` and the
corresponding `third_party/<name>/` source subtree.
