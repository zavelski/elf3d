# Elf3D 0.8.3

Elf3D 0.8.3 improves large-model interaction, produces smaller and more
readable glTF/GLB exports, and makes the viewer's model file dialogs faster to
use.

## Download

- `elf3d-viewer-0.8.3-windows-x64.zip`
- `SHA256SUMS.txt`

The viewer package is a ready-to-run Windows application. GitHub also provides
source archives for the `v0.8.3` tag.

## Highlights

- Raised the accepted glTF node count from 65,536 to 131,072 and ignored stale
  pointer samples outside the active viewport, preventing delayed clicks after
  a large-model stall from causing out-of-bounds picking errors.
- Reduced exported geometry size without losing index precision by selecting
  `UNSIGNED_BYTE`, `UNSIGNED_SHORT`, or `UNSIGNED_INT` independently for each
  primitive.
- Formatted standalone glTF JSON and GLB JSON chunks with tab indentation and
  compact scalar arrays while retaining exact binary32 tokens and opaque source
  metadata.
- Put Save As directly after Open in the toolbar, kept Reload in the File menu,
  and reduced Q/W/E/A/S/D navigation speed by half.
- Improved both model file dialogs with a visible text cursor, Save As
  double-click activation, and a file context menu for Open, Copy as path,
  Properties, EmEditor, Notepad, and Notepad++.
- Fixed clean parallel Visual Studio builds for grouped C++ module sources that
  share a basename.

## Compatibility

The supported public engine, model, and ImGui integration APIs are unchanged.
glTF/GLB export preserves the same supported model values while using more
compact index component types and human-readable JSON formatting.

## Requirements

- Windows x64.
- OpenGL 4.1 core-profile graphics driver.
- Microsoft Visual C++ Redistributable for Visual Studio 2022.

## License

Elf3D source is available under the MIT License. Third-party software retains
the licenses and notices included in the source tree and viewer package.
