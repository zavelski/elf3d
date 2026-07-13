# Elf3D 0.8.1

Elf3D 0.8.1 adds a production-style model file workflow to the Windows viewer
and exposes retained-document glTF/GLB export through the public engine facade.

## Download

- `elf3d-viewer-0.8.1-windows-x64.zip`
- `SHA256SUMS.txt`

The viewer package is a ready-to-run Windows application. GitHub also provides
source archives for the `v0.8.1` tag.

## Highlights

- Added a redesigned glTF asset browser and matching Save As dialog with path,
  search, locations, storage, recent folders, explicit file-type information,
  overwrite confirmation, and structured save errors.
- Added `Scene::save_model()` for exporting the canonical Document retained by
  a loaded scene as `.glb` or `.gltf`, backed by the existing transactional
  model writer.
- Remembered the last successfully opened or saved model directory in the
  platform user-configuration location and restored it across viewer launches.
- Prevented modal dialogs from leaking wheel or shortcut navigation into the
  3D viewport, and made the About dialog draggable with a visible scrollbar
  for expanded build details.

## Export Scope

Save As exports the retained model document. Runtime visibility, selection,
measurement, clipping, camera navigation, and procedural compatibility assets
remain viewer state and are not serialized.

## Requirements

- Windows x64.
- OpenGL 4.1 core-profile graphics driver.
- Microsoft Visual C++ Redistributable for Visual Studio 2022.

## License

Elf3D source is available under the MIT License. Third-party software retains
the licenses and notices included in the source tree and viewer package.
