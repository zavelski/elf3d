# Elf3D 0.7.7

Elf3D 0.7.7 refines interactive scene navigation with in-place look-around and
more immediate mouse-wheel movement after captured pointer gestures.

## Download

- `elf3d-viewer-0.7.7-windows-x64.zip`
- `SHA256SUMS.txt`

The viewer package is a ready-to-run Windows application. GitHub also provides
source archives for the `v0.7.7` tag.

## Highlights

- Added Space + left-drag look-around that rotates the camera while keeping its
  current world position fixed.
- Kept look-around active until the drag ends, even when Space is released
  during the gesture.
- Routed viewport wheel input directly from GLFW so zoom movement is applied
  without an extra queued render frame after captured navigation.
- Updated the viewer guide and control reference for the new interaction.

## Requirements

- Windows x64.
- OpenGL 4.1 core-profile graphics driver.
- Microsoft Visual C++ Redistributable for Visual Studio 2022.

## License

Elf3D source is available under the MIT License. Third-party software retains
the licenses and notices included in the source tree and viewer package.
