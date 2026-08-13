# Elf3D 0.10.2

Elf3D 0.10.2 is a focused Examine-mode navigation tuning release. It reduces
keyboard panning speed for more precise lateral and vertical movement while
retaining the forward/backward rate introduced in 0.10.1.

## Download

- `elf3d-viewer-0.10.2-windows-x64.zip`
- `SHA256SUMS.txt`

The viewer package is a ready-to-run Windows application. GitHub also provides
source archives for the `v0.10.2` tag.

## Highlights

- Halves the Examine-mode view-left/view-right pan rate for A and D.
- Halves the Examine-mode world-down/world-up pan rate for Q and E.
- Preserves frame-time scaling and the existing W/S keyboard dolly rate.
- Adds numeric navigation regressions for all four pan directions.

## Compatibility

This patch release does not change the public source API. Examine-mode A/D and
Q/E keyboard movement is intentionally slower; W/S movement is unchanged from
0.10.1. C++ clients should still be rebuilt with the same compiler toolchain
and runtime configuration as Elf3D, consistent with the documented
compatibility contract.

## Requirements

- Windows x64.
- OpenGL 4.1 core-profile graphics driver.
- Microsoft Visual C++ Redistributable for Visual Studio 2022.

## License

Elf3D source is available under the MIT License. Third-party software and
visual subjects retain the licenses and notices included in the source tree
and viewer package.
