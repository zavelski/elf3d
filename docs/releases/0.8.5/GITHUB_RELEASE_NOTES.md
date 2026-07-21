# Elf3D 0.8.5

Elf3D 0.8.5 makes the public C++ API more consistent and easier to integrate,
with one naming grammar, shared model/runtime value types, compile-checked
examples, and more precise camera-role diagnostics.

## Download

- `elf3d-viewer-0.8.5-windows-x64.zip`
- `SHA256SUMS.txt`

The viewer package is a ready-to-run Windows application. GitHub also provides
source archives for the `v0.8.5` tag.

## Highlights

- Standardized public operations around explicit roles and effects, including
  `create_model_entity()`, `create_perspective_camera_entity()`,
  `perspective_camera_description()`, `set_entity_local_visibility()`,
  `render_statistics()`, and `export_loaded_document()`.
- Consolidated identical model and runtime values into neutral public types in
  `elf3d/model_types.h`, including `AlphaMode`, `PixelFormat`, texture mapping
  and sampler values, `PerspectiveCameraDescription`, and `ModelLoadOptions`.
- Added six canonical examples that are compiled by the normal full-engine
  build: embedded rendering, load/report handling, procedural scene creation,
  picking and selection, Document round-tripping, and multiple viewports.
- Added a dedicated `entity_has_no_camera` error for valid entities used in
  camera-dependent operations without a camera role, with coverage across
  Scene, navigation, picking-ray, projection, and rendering paths.
- Strengthened module-graph, green-profile, and derived-policy enforcement with
  deterministic positive and negative fixtures.

## Compatibility

This release intentionally removes the superseded 0.8.4 public spellings rather
than retaining duplicate aliases. Source integrations must adopt the canonical
operation and value-type names described above. Ownership, host-driven frame
flow, synchronous threading, supported model formats, and viewer behavior are
unchanged.

## Requirements

- Windows x64.
- OpenGL 4.1 core-profile graphics driver.
- Microsoft Visual C++ Redistributable for Visual Studio 2022.

## License

Elf3D source is available under the MIT License. Third-party software and
visual subjects retain the licenses and notices included in the source tree
and viewer package.
