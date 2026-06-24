# Elf3D 0.2.0

Elf3D 0.2.0 updates the Windows reference viewer with GPU-assisted interaction
and a Low.3D-inspired light UI refresh while keeping GUI dependencies out of
the engine core.

## Supported Platform

- Windows x64.
- Visual Studio 2022.
- OpenGL 4.1 core-profile graphics driver.

Linux and macOS are not validated for this release.

## Included Download

- `elf3d-viewer-0.2.0-windows-x64.zip`
- `SHA256SUMS.txt`

SDK packaging is deferred until install/export rules and an external consumer
validation workflow exist.

## Highlights

- Public `elf3d` shared library with `Engine`, `Scene`, and `Viewport` facades.
- Dear ImGui/GLFW reference viewer with Droid Sans, light panels, generated PNG
  toolbar icons, compact status bar, and a Low.3D-like dock layout.
- Static glTF/GLB loading for bounded triangle geometry.
- Scene hierarchy, transforms, cameras, assets, persistent visibility, and
  viewport isolation.
- OpenGL 4.1 off-screen rendering with opaque metallic-roughness shading.
- Orbit/pan/dolly navigation, dynamic examine pivot, fit, and reset.
- GPU-first picking with CPU refinement/fallback, selection, isolation,
  distance measurement, section plane, and clipping boxes.
- Release ZIP includes required `assets/font` and `assets/icon` runtime assets.

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
- `docs/releases/0.2.0/`

## License

Elf3D original source code is licensed under the MIT License. Third-party
components remain governed by their own notices in `THIRD_PARTY.md` and
`third_party/licenses/`.
