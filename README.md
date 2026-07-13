# Elf3D

[![CI](https://github.com/zavelski/elf3d/actions/workflows/ci.yml/badge.svg)](https://github.com/zavelski/elf3d/actions/workflows/ci.yml)

Elf3D is a C++20 3D visualization engine and Windows desktop viewer for loading,
rendering, and inspecting static glTF 2.0 scenes.

## Download

Download the current Windows x64 package from the
[Elf3D 0.8.1 release](https://github.com/zavelski/elf3d/releases/tag/v0.8.1).
Extract the ZIP and run `elf3d_viewer.exe`.

Requirements:

- Windows x64;
- an OpenGL 4.1-capable graphics driver;
- Microsoft Visual C++ Redistributable for Visual Studio 2022.

The package includes the viewer, runtime library, required UI assets, user
guide, reference, and license notices.

## Features

- `.gltf` and `.glb` loading with structured compatibility diagnostics.
- Source-integrated static `elf3d_model` library with CPU-side
  `elf3d::Document` construction, material/image/texture/sampler storage,
  inspection, validation, primitive replacement, and glTF/GLB import/export
  APIs. Import/export retains all scenes and safe source image/JSON metadata.
- Scene hierarchy, transforms, perspective cameras, materials, textures,
  visibility, bounds, and model statistics.
- OpenGL 4.1 rendering with physically based material values, emissive and
  occlusion textures, unlit materials, transparency, and vertex color.
- Orbit, pan, dolly, fit, reset, mouse, and keyboard navigation.
- Picking, selection, hide/show, and viewport isolation.
- Point-to-point distance measurement.
- Section-plane and clipping-box inspection.
- Embeddable C++ engine API with host-owned window, event loop, OpenGL context,
  input, and presentation.

## Build From Source

Install Visual Studio 2022 with the Desktop development with C++ workload and
CMake 3.28 or newer. From the repository root:

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug --parallel
ctest --preset windows-debug --output-on-failure
```

Run the viewer:

```powershell
.\out\build\windows-debug\bin\Debug\elf3d_viewer.exe
```

Open a model from **File > Open...**, pass its path as the first command-line
argument, or drop it onto the viewer. Use **File > Save As...** to export the
loaded model as `.gltf` or `.glb`.

Release commands, output paths, and troubleshooting are in
[`docs/BUILDING.md`](docs/BUILDING.md).

For the model-only static library and tests:

```powershell
cmake --preset windows-model-debug
cmake --build --preset windows-model-debug --parallel
ctest --preset windows-model-debug --output-on-failure
```

## Repository Layout

```text
include/elf3d/       Public C++ headers
facade/elf3d/        Shared-library entry points
modules/             Model, engine, import, rendering source, and focused tests
integrations/imgui/  Dear ImGui presentation integration
apps/viewer/         Desktop viewer and runtime assets
tests/               Integration tests and distributable fixture
cmake/               Build configuration
third_party/         Vendored dependencies and license notices
docs/                User and developer documentation
```

## Documentation

- [Practical Viewer Guide](docs/GUIDE.md)
- [Viewer Reference](docs/VIEWER.md)
- [Building](docs/BUILDING.md)
- [Testing](docs/TESTING.md)
- [C++ API Guide](docs/PUBLIC_API.md)
- [glTF Compatibility](docs/GLTF.md)
- [Rendering Reference](docs/RENDERING.md)
- [Support](SUPPORT.md)

## License

Elf3D source is available under the MIT License in [`LICENSE`](LICENSE).
Vendored dependencies and assets retain their respective licenses; see
[`THIRD_PARTY.md`](THIRD_PARTY.md).
