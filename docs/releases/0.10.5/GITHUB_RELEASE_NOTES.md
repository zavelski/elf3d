# Elf3D 0.10.5

Elf3D 0.10.5 improves glTF material fidelity and viewer responsiveness. It
adds full tangent-space normal mapping, a calibrated high-contrast studio
environment, broader compatibility for imperfect and legacy assets, and a
faster empty-interface startup path.

## Download

- `elf3d-viewer-0.10.5-windows-x64.zip`
- `SHA256SUMS.txt`

The viewer package is a ready-to-run Windows application. GitHub also provides
source archives for the `v0.10.5` tag.

## Highlights

- Renders glTF normal textures using authored tangents or deterministic
  MikkTSpace generation, including UV selection, texture transforms, normal
  scale, mirrored transforms, and double-sided surfaces.
- Adds embedded studio IBL v3 with shaped softbox reflections, calibrated mean
  radiance, lazy first-use GPU upload, and Standard tone mapping as the default
  display path.
- Regenerates unusable zero-length tangents instead of rejecting otherwise
  renderable models.
- Opens legacy models that require `KHR_materials_pbrSpecularGlossiness`
  through a documented metallic-roughness approximation and reports the
  unsupported specular-glossiness texture as a compatibility diagnostic.
- Presents the empty viewer interface before loading a command-line model,
  removes the procedural startup cube, and uses the light-blue default clear
  color `(213, 227, 240, 255)`.
- Halves the default mouse-orbit sensitivity and labels the 3D viewport tab
  with the loaded model filename and extension.

## Compatibility

The source-integrated model API adds `Float4` and optional tangent storage to
`PrimitiveData` and `PrimitiveDataView`. Imported and generated tangents are
retained during glTF round trips. Programmatic meshes created through the
stable DLL-facing mesh API remain geometric-normal-only.

The default Standard PBR presentation now uses environment intensity `2.0`,
exposure `0 EV`, Standard tone mapping, and the calibrated studio IBL v3. PBR
Neutral and the diagnostic no-tone-map mode remain selectable.

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
