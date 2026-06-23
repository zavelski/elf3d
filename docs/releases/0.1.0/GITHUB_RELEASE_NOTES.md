# Elf3D 0.1.0

Elf3D 0.1.0 is the first audited public baseline of the Elf3D C++20
visualization engine.

## Supported Platform

- Windows x64.
- Visual Studio 2022.
- OpenGL 4.1 core-profile graphics driver.

Linux and macOS are not validated for this release.

## Included Download

- `elf3d-viewer-0.1.0-windows-x64.zip`
- `SHA256SUMS.txt`

SDK packaging is deferred until install/export rules and an external consumer
validation workflow exist.

## Highlights

- Public `elf3d` shared library with `Engine`, `Scene`, and `Viewport` facades.
- Dear ImGui/GLFW reference viewer.
- Static glTF/GLB loading for bounded triangle geometry.
- Scene hierarchy, transforms, cameras, assets, and visibility.
- OpenGL 4.1 off-screen rendering with opaque metallic-roughness shading.
- Orbit/pan/dolly navigation.
- CPU picking, selection, isolation, distance measurement, section plane, and
  clipping boxes.

## Known Limitations

- Windows/OpenGL is the only validated platform/backend.
- The public DLL surface is a C++ API and not a stable C ABI.
- Rendering is opaque-only; alpha mask and alpha blend are not rendered.
- No animation, skins, morph targets, compression extensions, KTX2, cameras,
  lights, normal maps, occlusion, emissive maps, shadows, or image-based
  lighting.
- Scene mutation and rendering are single-threaded.
- OpenGL resources require a compatible context during shutdown.

## Documentation

- `README.md`
- `docs/PUBLIC_API_OVERVIEW.md`
- `docs/GLTF_SUPPORT.md`
- `docs/RENDERING_PIPELINE.md`
- `docs/VIEWPORT_AND_TOOLS.md`
- `docs/LIFETIME_AND_THREADING.md`
- `docs/TESTING.md`

## License

Elf3D original source code is licensed under the MIT License. Third-party
components remain governed by their own notices in `THIRD_PARTY.md` and
`third_party/licenses/`.
