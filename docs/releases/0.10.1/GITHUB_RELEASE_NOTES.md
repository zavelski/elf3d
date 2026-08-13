# Elf3D 0.10.1

Elf3D 0.10.1 is a focused viewer and navigation correction release following
the Foundation 2.0 architecture transition. It restores the intended viewer
font and adjusts Examine-mode keyboard movement based on post-transition
review.

## Download

- `elf3d-viewer-0.10.1-windows-x64.zip`
- `SHA256SUMS.txt`

The viewer package is a ready-to-run Windows application. GitHub also provides
source archives for the `v0.10.1` tag.

## Highlights

- Restores the bundled `DroidSans.ttf` as the viewer's primary and default
  presentation font, with a smoke check that enforces the configured default.
- Doubles the Examine-mode keyboard dolly rate for both W and S while retaining
  frame-time scaling and local working-depth behavior.

## Compatibility

This patch release does not change the public source API. Examine-mode W/S
keyboard movement is intentionally faster. C++ clients should still be rebuilt
with the same compiler toolchain and runtime configuration as Elf3D, consistent
with the documented compatibility contract.

## Requirements

- Windows x64.
- OpenGL 4.1 core-profile graphics driver.
- Microsoft Visual C++ Redistributable for Visual Studio 2022.

## License

Elf3D source is available under the MIT License. Third-party software and
visual subjects retain the licenses and notices included in the source tree
and viewer package.
