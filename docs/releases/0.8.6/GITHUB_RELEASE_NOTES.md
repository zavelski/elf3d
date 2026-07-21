# Elf3D 0.8.6

Elf3D 0.8.6 hardens public ownership and scene-loading contracts while making
the model and rendering surfaces smaller and more explicit.

## Download

- `elf3d-viewer-0.8.6-windows-x64.zip`
- `SHA256SUMS.txt`

The viewer package is a ready-to-run Windows application. GitHub also provides
source archives for the `v0.8.6` tag.

## Highlights

- Made `Engine::load_scene()` the single scene-loading operation. It now
  returns `LoadedScene`, containing both the Scene and its structured
  compatibility report, and preserves `metadata_not_preserved` diagnostics
  without fallback relabeling.
- Replaced address-derived Engine and Document owner identities with nonzero
  owner tokens, preventing stale IDs from becoming valid when storage
  addresses are reused.
- Added distinct `invalid_*_id` error codes for all eight persistent Document
  identity families while preserving the numeric values of existing error
  codes and the separate runtime Entity/Handle failure vocabulary.
- Made `Document` the single public construction and mutation owner. Importer
  staging remains private, and validated `replace_primitive()` replaces
  retained mutable geometry spans plus manual bounds refresh.
- Added the focused `elf3d/rendering.h` contract, moved neutral overlay values
  to `graphics.h`, and kept the umbrella `elf3d.h` include convenient for
  existing hosts.
- Confined GLM to the math boundary, converted navigation and picking to
  project-owned value types, removed shared mutable fake-device state, and
  consolidated repeated bounds and framebuffer-clear operations.

## Compatibility

This release intentionally removes duplicate or unsafe source-level API
shapes:

- Replace `load_scene_with_report(path)` with `load_scene(path)`. The latter
  now returns `Result<LoadedScene>` rather than
  `Result<std::unique_ptr<Scene>>`; access the Scene through
  `loaded.value().scene`.
- Construct `Document` directly instead of using the former public
  `DocumentBuilder`; call `validate_document()` when explicit completion-time
  validation is required.
- Copy geometry into `PrimitiveData` and call `replace_primitive()` instead of
  using the removed `mutable_positions()`, `mutable_normals()`, and
  `update_primitive_bounds()` operations.

The `rendering.h` split is additive, and `elf3d.h` continues to include the
complete shared-engine surface. Ownership direction, host-driven frame flow,
synchronous threading, supported model formats, and viewer interaction remain
unchanged.

## Requirements

- Windows x64.
- OpenGL 4.1 core-profile graphics driver.
- Microsoft Visual C++ Redistributable for Visual Studio 2022.

## License

Elf3D source is available under the MIT License. Third-party software and
visual subjects retain the licenses and notices included in the source tree
and viewer package.
