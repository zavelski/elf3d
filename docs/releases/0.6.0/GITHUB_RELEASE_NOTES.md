# Elf3D 0.6.0

Purpose: Provide release notes for a future Elf3D 0.6.0 GitHub Release.

Applicable version: 0.6.0

Document status: Draft release notes pending publication gates.

Release source identifier: local tag `v0.6.0` after release commit.

Implementation source paths: `CHANGELOG.md`, `README.md`, `include/elf3d`,
`modules`, `third_party`, `apps/viewer`, `scripts/package_release.ps1`

Known limitations: These notes describe a locally prepared release source. No
GitHub Release, published asset, or public clone has been verified.

Related documents: `RELEASE_CHECKLIST.md`, `VALIDATION_SUMMARY.md`,
`KNOWN_LIMITATIONS.md`, `RELEASE_ARTIFACTS.md`

Elf3D 0.6.0 makes the repository self-contained for the currently used
third-party source dependencies and records the current static glTF/GLB
compatibility baseline. The public product remains one `elf3d` shared library
plus the reference viewer.

## Supported Platform

- Windows x64.
- Visual Studio 2022 v17.14.35 or newer.
- OpenGL 4.1 core-profile graphics driver.

Linux and macOS are not validated for this release.

## Included Download

- `elf3d-viewer-0.6.0-windows-x64.zip`
- `SHA256SUMS.txt`

SDK packaging remains deferred until install/export rules and an external
consumer validation workflow exist.

## Highlights

- Vendored the used source subsets for Dear ImGui, GLFW, GLM, GLAD, cgltf, and
  stb under `third_party/` as ordinary tracked files.
- Normal configure/build no longer needs FetchContent, network downloads,
  external clones, or `_deps/*-src` dependency source trees.
- Third-party notices are preserved with each vendored dependency and copied
  into the viewer package under `third_party_licenses/`.
- Existing C++20 named-module and internal OBJECT-library architecture remains
  intact; the external product remains `elf3d.dll`.
- glTF/GLB coverage includes bounded static geometry, node hierarchy, TRS and
  matrix transforms, external/data/GLB buffers, PNG/JPEG textures, samplers,
  UV0/UV1, vertex color, material texture-coordinate selection,
  `KHR_texture_transform`, base-color alpha values, and scene-load reporting.
- Added/kept local validation around public API versioning, internal module
  imports, glTF corpus probing, release packaging, and archive contents.

## Known Limitations

- Windows/OpenGL is the only locally validated platform/backend.
- The public DLL surface is a C++ API and not a stable C ABI.
- Named-module BMI/IFC artifacts are internal build products, not SDK assets.
- Rendering remains a bounded static visualization path; full glTF 2.0 material,
  animation, skinning, morphing, camera, light, compression, and KTX2 support is
  not implemented.
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
- `docs/releases/0.6.0/`

## License

Elf3D original source code is licensed under the MIT License. Third-party
components remain governed by their own notices in `THIRD_PARTY.md` and the
corresponding `third_party/<name>/` source subtree.
