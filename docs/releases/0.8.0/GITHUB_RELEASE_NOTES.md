# Elf3D 0.8.0

Elf3D 0.8.0 introduces a source-integrated CPU-side model library while
preserving the existing engine, ImGui integration, and Windows viewer products.
The new `elf3d_model` target provides canonical Document ownership and glTF/GLB
import and export without configuring rendering, windowing, or viewer targets.

## Download

- `elf3d-viewer-0.8.0-windows-x64.zip`
- `SHA256SUMS.txt`

The viewer package is a ready-to-run Windows application. GitHub also provides
source archives for the `v0.8.0` tag.

## Highlights

- Added `elf3d_model` and `elf3d::Document` for CPU-side model construction,
  inspection, validation, primitive processing, and source-integrated static
  linking.
- Added glTF/GLB Document export and model-first importing that retains all
  scenes, authored default-scene selection, source-image fidelity, and bounded
  raw metadata when safe to preserve.
- Derived engine Scene state from retained Document data and updated renderer,
  picking, measurement, clipping, test coverage, and public documentation for
  the model-first architecture.
- Added model-only CMake presets and a configured-target dependency check that
  excludes engine, graphics, windowing, ImGui, and viewer targets.

## Requirements

- Windows x64.
- OpenGL 4.1 core-profile graphics driver.
- Microsoft Visual C++ Redistributable for Visual Studio 2022.

## License

Elf3D source is available under the MIT License. Third-party software retains
the licenses and notices included in the source tree and viewer package.
