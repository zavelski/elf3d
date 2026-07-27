# Elf3D 0.9.0

Elf3D 0.9.0 strengthens the project's reproducible Windows build and validation
contracts. Runtime behavior and public C++ interfaces are unchanged from
0.8.9.

## Download

- `elf3d-viewer-0.9.0-windows-x64.zip`
- `SHA256SUMS.txt`

The viewer package is a ready-to-run Windows application. GitHub also provides
source archives for the `v0.9.0` tag.

## Highlights

- Adds checked-in full-engine and CPU-only model presets for Debug and Release,
  with automated checks that verify the expected target composition.
- Validates the full Windows product and model-only library independently in CI
  so CPU-only changes do not require OpenGL test execution.
- Adds scheduled and manually dispatched Release validation alongside the
  push- and pull-request Debug jobs.
- Pins the Windows CI toolchain to CMake 4.3.4 and clang-format 22.1.8 and
  enforces formatting across project-owned C++ sources.
- Makes the non-CTest render benchmark explicitly opt-in and documents the
  build, testing, and contributor contracts enforced by automation.

## Compatibility

The supported public APIs and glTF/GLB behavior are unchanged in this release.
C++ clients should still be rebuilt with the same compiler toolchain and runtime
configuration as Elf3D, consistent with the documented compatibility contract.

## Requirements

- Windows x64.
- OpenGL 4.1 core-profile graphics driver.
- Microsoft Visual C++ Redistributable for Visual Studio 2022.

## License

Elf3D source is available under the MIT License. Third-party software and
visual subjects retain the licenses and notices included in the source tree
and viewer package.
