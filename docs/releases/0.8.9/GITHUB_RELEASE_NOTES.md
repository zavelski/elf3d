# Elf3D 0.8.9

Elf3D 0.8.9 improves compatibility with deeply nested, multi-gigabyte, and
texture-heavy glTF/GLB scenes while keeping importer resource use bounded.

## Download

- `elf3d-viewer-0.8.9-windows-x64.zip`
- `SHA256SUMS.txt`

The viewer package is a ready-to-run Windows application. GitHub also provides
source archives for the `v0.8.9` tag.

## Highlights

- Raises the glTF hierarchy limit from 1,024 to 8,192 levels and validates the
  iterative Document-to-Scene path with a 5,120-level regression model.
- Raises source-file, embedded GLB BIN, and total-buffer limits to 3 GiB while
  retaining the 1 GiB limit for each external buffer.
- Recovers overflowed signed 32-bit GLB buffer sizes and offsets only when a
  single embedded BIN chunk and its complete sequential, aligned buffer-view
  layout prove the intended unsigned values. Successful recovery is reported
  through a structured load diagnostic.
- Uses `LINEAR_MIPMAP_LINEAR` for imported glTF textures whose sampler or
  `minFilter` is absent. Explicit author filters remain unchanged, and generated
  mip levels are included in resident-texture byte statistics.
- Raises the total decoded RGBA8 image budget from 512 MiB to 2 GiB in 64-bit
  builds. The 32-bit budget remains 512 MiB.

## Public Interface Addition

`ModelLoadDiagnosticCode` and `SceneLoadDiagnosticCode` add
`repaired_signed_buffer_layout` so applications can identify the narrow large-
GLB compatibility recovery. Rebuild C++ clients against the 0.8.9 headers and
library, consistent with Elf3D's documented same-toolchain C++ compatibility
contract.

## Requirements

- Windows x64.
- OpenGL 4.1 core-profile graphics driver.
- Microsoft Visual C++ Redistributable for Visual Studio 2022.

## License

Elf3D source is available under the MIT License. Third-party software and
visual subjects retain the licenses and notices included in the source tree
and viewer package.
