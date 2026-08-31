# Elf3D 0.10.6

Elf3D 0.10.6 is a validation-reliability hotfix. It prevents transient load on
a hosted Windows runner from falsely failing the unoptimized, byte-exact studio
environment verification.

## Download

- `elf3d-viewer-0.10.6-windows-x64.zip`
- `SHA256SUMS.txt`

The viewer package is a ready-to-run Windows application. GitHub also provides
source archives for the `v0.10.6` tag.

## Changes

- Extends the full Debug studio-environment verification timeout from two to
  five minutes while preserving the same deterministic bake and byte-for-byte
  acceptance criteria.

## Compatibility

This hotfix does not change rendering behavior or the public C++ interface.
The C++ interface remains toolchain-sensitive rather than a stable binary ABI;
clients should rebuild with the same compiler toolchain and runtime
configuration as Elf3D.

## Requirements

- Windows x64.
- OpenGL 4.1 core-profile graphics driver.
- Microsoft Visual C++ Redistributable for Visual Studio 2022.

## License

Elf3D source is available under the MIT License. Third-party software and
visual subjects retain the licenses and notices included in the source tree
and viewer package.
