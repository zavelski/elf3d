# Elf3D 0.11.5

Elf3D 0.11.5 improves glass, polished surfaces and layered materials in the
standard renderer while retaining its HDR studio lighting and tone mapping.

## Changes

- `KHR_materials_transmission` adds optical transparency independently of
  `alphaMode`. A linear scene-color snapshot supplies roughness-filtered
  refraction and base-color tint.
- `KHR_materials_volume` applies authored thickness and Beer–Lambert absorption
  using the complete instance transform, including nonuniform and mirrored
  scale. Authored optical thickness remains distinct from geometric thickness.
- `KHR_materials_clearcoat` adds direct and environment reflection, attenuation
  of the underlying layer, factor/roughness textures and independent normal
  mapping. Specular strength/color textures are evaluated with their defined
  channels and color spaces. Extension slots support UV0/UV1 and texture transforms.
- Correct GGX narrow highlights, specular-color clamp order and the IOR-zero
  compatibility case. Existing normal mapping and indirect multiple-scattering
  compensation remain part of the standard PBR renderer.
- Sort blended primitives by camera-space depth of their geometry bounds,
  with deterministic ties. Optical transmission preserves authored alpha coverage.
- Import/export retains typed material factors, textures and mappings through
  glTF/GLB round trips. Public material structures have changed; rebuild C++
  applications consuming the headers.
- Viewport statistics report scene-color copy count and transmission storage.
  The snapshot is allocated lazily and reused, with at most one copy per rendered
  viewport frame and no extra geometry pass. Capture tools accept explicit
  projection/background settings; benchmark CSV schema 3 adds optical counters.

## Windows package

Download `elf3d-viewer-0.11.5-windows-x64.zip` and verify it against
`SHA256SUMS.txt`. Extract the complete archive and run `elf3d_viewer.exe` or:

```text
elf3d_cmd.exe --input model.glb --output rotated.glb --rotate 0 0 90
```

Keep `assets` beside the Viewer executable. The ZIP contains both applications,
`elf3d.dll`, guides and license notices; it is a runnable application package.

## Requirements and limitations

- Windows x64 and Microsoft Visual C++ v14 x64 Redistributable at least as recent
  as the MSVC 19.51 build tools. Viewer requires an OpenGL 4.1 core-profile driver;
  Cmd needs no graphics driver.
- Source builds use Visual Studio 2026 with v145, CMake 4.4.3 and PowerShell
  7.6.5 or newer. Other platforms remain unvalidated portability targets.
- The Model SDK is source-integrated. The C++ DLL API does not promise a stable
  ABI across toolchains, and the ZIP is not a binary C++ SDK.
- Transmission uses a local slab approximation and an opaque/masked scene
  snapshot. Exact mesh exits, multiple refracting layers, interior-camera volume
  transport and caustics are outside this rendering contract. Primitive sorting
  cannot exactly order intersecting transparent triangles.
- Supported textures retain the documented image-format and UV0/UV1 limits.
  A 1600×900 optical viewport uses approximately 14.65 MiB for its RGBA16F mip chain,
  excluding driver overhead.

Elf3D source is licensed under MIT. Third-party software retains its own terms
and notices in `THIRD_PARTY.md` and `third_party_licenses`.
