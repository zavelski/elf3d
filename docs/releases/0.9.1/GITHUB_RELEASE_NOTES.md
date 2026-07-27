# Elf3D 0.9.1

Elf3D 0.9.1 corrects the public model-only validation path introduced in
0.9.0 and strengthens the release gate for generated public source snapshots.
Runtime behavior and public C++ interfaces are unchanged.

## Download

- `elf3d-viewer-0.9.1-windows-x64.zip`
- `SHA256SUMS.txt`

The viewer package is a ready-to-run Windows application. GitHub also provides
source archives for the `v0.9.1` tag.

## Highlights

- Keeps the `elf3d.model_only_dependencies` checker under the public `cmake/`
  tree so the independent CPU-only CTest suite works from the generated public
  source snapshot.
- Configures, builds, and tests both full-engine and model-only Release presets
  from the allowlisted public snapshot before a public commit or tag is pushed.
- Caps publication builds at four parallel jobs, matching the bounded Windows
  CI configuration.
- Gives the conditional Release matrix a stable label when it is skipped on
  ordinary push and pull-request workflows.

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
