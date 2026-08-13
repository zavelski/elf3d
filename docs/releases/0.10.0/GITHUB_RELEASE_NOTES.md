# Elf3D 0.10.0

Elf3D 0.10.0 completes the Foundation 2.0 architecture transition. It adds a
canonical framework-owned desktop lifecycle, preserves host-owned composition
through an explicit embedding product, and keeps concrete application Tools
and Workflows in the reference viewer instead of the Runtime SDK.

## Download

- `elf3d-viewer-0.10.0-windows-x64.zip`
- `SHA256SUMS.txt`

The viewer package is a ready-to-run Windows application. GitHub also provides
source archives for the `v0.10.0` tag.

## Highlights

- Adds `elf3d_app` / `elf3d::app` with `run_application()`, normalized input,
  owner-scoped interaction arbitration, queued viewport rendering, final
  presentation, and deterministic teardown.
- Adds `elf3d_embed` / `elf3d::embed` with `EmbeddedRuntime` for hosts that must
  own their window, OpenGL context, event loop, input, presentation, and
  shutdown.
- Refocuses the Runtime SDK on general Scene and Viewport mechanisms, including
  persistent surface anchors, world-to-viewport projection, anchor visibility,
  explicit clipping state, and bounded generic overlays.
- Moves concrete measurement, selection, clipping presentation, gesture, and
  workflow policy into viewer-owned Tools and Workflows.
- Reorganizes the reference viewer around explicit Components, typed command
  dispatch, bounded file Workflows, and separately owned application state.
- Adds an external public-only Measurement Tool copy/adapt regression plus
  broader application lifecycle, input, interaction, viewer, and preset
  validation.

## Compatibility

This release contains source-level Runtime SDK changes for applications built
against 0.9.1:

- Standard desktop applications should link `elf3d::app`, implement
  `elf3d::Application`, and enter through `elf3d::run_application()`.
- Host-owned integrations should link `elf3d::embed` and create
  `elf3d::EmbeddedRuntime`; public `EngineConfiguration`, `Engine::create()`,
  and `Engine::native_texture_view()` are no longer the composition boundary.
- `ViewportInput` is replaced by mechanism-focused `NavigationInput`, and the
  core-owned measurement Tool API has been removed. Applications compose their
  own Tool policy from public picking, surface-anchor, projection, visibility,
  and overlay operations.
- Selection activation and clipping helper presentation are application policy
  rather than Runtime SDK state.

C++ clients should be rebuilt with the same compiler toolchain and runtime
configuration as Elf3D, consistent with the documented compatibility contract.

## Requirements

- Windows x64.
- OpenGL 4.1 core-profile graphics driver.
- Microsoft Visual C++ Redistributable for Visual Studio 2022.

## License

Elf3D source is available under the MIT License. Third-party software and
visual subjects retain the licenses and notices included in the source tree
and viewer package.
