# Elf3D 0.8.2

Elf3D 0.8.2 is a structural quality release that completes the current
green-profile metric cleanup and divides large implementation areas into
focused, boundary-owned units without changing the supported product surface.

## Download

- `elf3d-viewer-0.8.2-windows-x64.zip`
- `SHA256SUMS.txt`

The viewer package is a ready-to-run Windows application. GitHub also provides
source archives for the `v0.8.2` tag.

## Highlights

- Removed the remaining Essential C++ Lite metric allowances and reduced the
  reviewed boundary allowance set to 23.
- Split canonical Document storage, mutation, validation, views, navigation
  updates, renderer tests, viewport picking, and related tests into focused
  implementation and support units.
- Divided the Windows viewer host by responsibility across runtime lifetime,
  assets, browsing, chrome, panels, preferences, status, scene panels, and
  viewport interaction.
- Divided the OpenGL adapter into focused device state, target, resource,
  indexed-draw, overlay, and picking units while keeping native graphics types
  private to the backend boundary.
- Divided glTF import and export into focused validation, asset, material,
  texture, geometry, document, image, JSON emission, and writer units while
  retaining the existing canonical Document contract.

## Compatibility

This release preserves the supported public engine, model, ImGui integration,
viewer behavior, package layout, and glTF/GLB workflow. The changes are focused
on internal ownership, dependency visibility, maintainability, and regression
coverage.

## Requirements

- Windows x64.
- OpenGL 4.1 core-profile graphics driver.
- Microsoft Visual C++ Redistributable for Visual Studio 2022.

## License

Elf3D source is available under the MIT License. Third-party software retains
the licenses and notices included in the source tree and viewer package.
