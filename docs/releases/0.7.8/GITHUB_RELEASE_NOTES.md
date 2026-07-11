# Elf3D 0.7.8

Elf3D 0.7.8 is a source-build maintenance release. It reorganizes internal C++
module build grouping to keep generated Visual Studio solutions smaller while
preserving the public engine API, existing C++ module names, and viewer
behavior.

## Download

- `elf3d-viewer-0.7.8-windows-x64.zip`
- `SHA256SUMS.txt`

The viewer package is a ready-to-run Windows application. GitHub also provides
source archives for the `v0.7.8` tag.

## Highlights

- Replaced one production CMake project per named C++ module with grouped
  OBJECT targets for foundation, domain, image, glTF, graphics, OpenGL, tools,
  and view code.
- Preserved the public C++ API and all existing internal C++ module names.
- Preserved existing CTest coverage and test names while consolidating focused
  module tests behind grouped executables.
- Kept image decoding, glTF import, OpenGL backend, ImGui integration, and the
  viewer as separate boundary-sensitive build areas.

## Requirements

- Windows x64.
- OpenGL 4.1 core-profile graphics driver.
- Microsoft Visual C++ Redistributable for Visual Studio 2022.

## License

Elf3D source is available under the MIT License. Third-party software retains
the licenses and notices included in the source tree and viewer package.
