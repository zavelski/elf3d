# Elf3D 0.7.1

Purpose: Provide release notes for a future Elf3D 0.7.1 GitHub Release.

Applicable version: 0.7.1

Document status: Draft release notes pending publication gates.

Last verified Git commit: local tag `v0.7.1` after release commit

Implementation source paths: `CHANGELOG.md`, `README.md`, `include/elf3d`,
`modules`, `third_party`, `apps/viewer`, `scripts/package_release.ps1`

Known limitations: These notes describe a locally prepared release source. No
GitHub Release, published asset, or public clone has been verified.

Related documents: `RELEASE_CHECKLIST.md`, `VALIDATION_SUMMARY.md`,
`KNOWN_LIMITATIONS.md`, `RELEASE_ARTIFACTS.md`

Elf3D 0.7.1 prepares a local release baseline on top of the current static
glTF/GLB compatibility work and stabilizes important reference-viewer viewport
interactions. It also fixes the transparency color-space path and tightens
importer resource limits. The public product remains one `elf3d` shared
library plus the reference viewer.

## Supported Platform

- Windows x64.
- Visual Studio 2022 v17.14.35 or newer.
- OpenGL 4.1 core-profile graphics driver.

Linux and macOS are not validated for this release.

## Included Download

- `elf3d-viewer-0.7.1-windows-x64.zip`
- `SHA256SUMS.txt`

SDK packaging remains deferred until install/export rules and an external
consumer validation workflow exist.

## Highlights

- About dialog positioning is centered on the first and later opens by using
  Dear ImGui next-window positioning before the dialog first renders.
- Mouse-wheel dolly works when the cursor is inside the 3D view even if the
  docked 3D view did not already have keyboard/window focus.
- Quick click, mouse movement, and then wheel zoom no longer applies a stale
  off-axis examine pivot that rotates or jumps the camera.
- Transparent material blending now happens in the linear off-screen render
  target before the OpenGL display resolve applies sRGB transfer encoding.
- The glTF importer now checks the final expanded triangle-list index count for
  strips and fans before loading external buffers.
- `Engine::load_scene()` no longer writes import warnings to a global stream;
  hosts use `Engine::load_scene_with_report()` to surface diagnostics.
- A real hidden-context OpenGL smoke test covers GLSL compilation and one
  transparent pixel result in automation when OpenGL 4.1 is available.
- Existing C++20 named-module and internal OBJECT-library architecture remains
  intact; the external product remains `elf3d.dll`.
- glTF/GLB coverage includes bounded static geometry, node hierarchy, TRS and
  matrix transforms, external/data/GLB buffers, PNG/JPEG textures, samplers,
  UV0/UV1, vertex color, material texture-coordinate selection,
  `KHR_texture_transform`, base-color alpha values, and scene-load reporting.
- Local validation covers public API versioning, internal module imports, glTF
  importer behavior, renderer material propagation, a real OpenGL shader/pixel
  smoke, navigation regression coverage, release packaging, and archive
  contents.

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
- `docs/releases/0.7.1/`

## License

Elf3D original source code is licensed under the MIT License. Third-party
components remain governed by their own notices in `THIRD_PARTY.md` and the
corresponding `third_party/<name>/` source subtree.
