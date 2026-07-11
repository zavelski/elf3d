# Elf3D 0.7.9

Elf3D 0.7.9 is a source-build maintenance release. It advances the internal
Essential C++ Lite 1.2 migration, tightens green-profile enforcement, and
continues splitting large implementation files while preserving the public
engine, ImGui integration, and viewer product boundaries.

## Download

- `elf3d-viewer-0.7.9-windows-x64.zip`
- `SHA256SUMS.txt`

The viewer package is a ready-to-run Windows application. GitHub also provides
source archives for the `v0.7.9` tag.

## Highlights

- Added a machine-readable Essential C++ Lite 1.2 green-profile baseline with
  stricter module validation and expanded negative enforcement fixtures.
- Split large facade, navigation, picking, renderer, scene, and viewport
  implementation areas into narrower source files without changing the product
  dependency boundaries.
- Updated public API ownership, result/error, scene loading, viewport, and
  validation documentation for the current same-toolchain C++ SDK contract.
- Preserved the public engine library, optional Dear ImGui integration, and
  reference viewer packaging model.

## Requirements

- Windows x64.
- OpenGL 4.1 core-profile graphics driver.
- Microsoft Visual C++ Redistributable for Visual Studio 2022.

## License

Elf3D source is available under the MIT License. Third-party software retains
the licenses and notices included in the source tree and viewer package.
