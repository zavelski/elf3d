# Elf3D

[![CI](https://github.com/zavelski/elf3d/actions/workflows/ci.yml/badge.svg)](https://github.com/zavelski/elf3d/actions/workflows/ci.yml)

Elf3D is a modular C++20 3D visualization engine for loading, rendering, and
interactively inspecting glTF 2.0 scenes in desktop applications.

Current development version: 0.7.1.

Maturity: first public baseline. The project is useful as an embeddable
Windows/OpenGL visualization slice and reference viewer, but it is not a
complete game engine, editor, or stable cross-toolchain binary SDK.

Validated baseline: Windows desktop x64, Visual Studio 2022 v17.14.35, MSVC
19.44.35228.0, dynamic MSVC runtime, and OpenGL 4.1 core profile. Linux and
macOS are architectural goals, but they are not validated platforms for 0.7.1.

## Features

- Public `elf3d` shared library with `Engine`, `Scene`, and `Viewport` facades.
- Optional `elf3d_imgui` host-integration library.
- `elf3d_viewer` reference application and graphical testbed.
- Static glTF/GLB loading with UV0/UV1, texture transforms, practical material
  diagnostics, triangle strips/fans, and perspective cameras.
- Scene hierarchy, transforms, cameras, mesh/material/image assets, and bounds.
- OpenGL 4.1 off-screen viewport rendering with metallic-roughness, emissive,
  occlusion, unlit, alpha-mask, and simple linear alpha-blend paths.
- Orbit, pan, wheel/drag dolly, dynamic examine pivot, fit, and reset navigation.
- GPU-first picking with CPU refinement/fallback, one selected entity per viewport, visibility, and isolation.
- One point-to-point distance measurement per viewport.
- One section plane and up to three axis-aligned clipping boxes per viewport.

## Architecture

The host application owns the native window, event loop, application main loop,
Dear ImGui context, GUI construction, and final frame presentation. Elf3D owns
engine state, scenes, viewports, rendering services, graphics resources, and
tool state.

The engine core does not depend on Dear ImGui, GLFW, or application GUI code.
GLM, cgltf, GLAD, OpenGL types, and private module classes do not appear in the
public Elf3D headers.

Internal engine build grouping uses CMake OBJECT libraries linked into the
public `elf3d` shared library. Each internal engine OBJECT library exposes a
C++20 named-module interface for its logical boundary; module export is not DLL
symbol export, and the public DLL ABI remains controlled by explicit
`ELF3D_API` declarations. The reference viewer uses only the public Elf3D API
plus the optional ImGui integration target.

## glTF Scope

Elf3D 0.7.1 supports `.gltf` and `.glb`, external/data/GLB buffers, PNG/JPEG
images, node hierarchy, TRS/matrices, reusable meshes, triangle lists/strips/
fans, positions, normals, generated normals, `TEXCOORD_0`, `TEXCOORD_1`,
`COLOR_0`, per-slot `texCoord`, `KHR_texture_transform`, base-color alpha,
`OPAQUE`/`MASK`/simple linear `BLEND`, emissive and occlusion textures, unlit,
emissive strength, IOR/specular factors, mesh quantization, and perspective
cameras. `Engine::load_scene_with_report` exposes structured compatibility
diagnostics to hosts.

Normal textures are imported and preserved but not rendered until a complete
tangent-space path exists. Animation, skin/morph deformation, scene lights,
orthographic cameras, layered/transmissive materials, Draco, meshopt, KTX2/
BasisU, shadows, IBL, scene editing, runtime plugins, and a stable C ABI remain
unsupported or diagnostic fallbacks. See the support matrix for exact behavior.

## Requirements

- Windows x64.
- Visual Studio 2022 v17.14.35 or newer with the Desktop development with C++
  workload.
- CMake 3.28 or newer. Visual Studio 2022 v17.14.35 provides CMake
  3.31.6-msvc6, which is the current validated local tool.
- Graphics driver supporting an OpenGL 4.1 core-profile context.

## Build

From a Visual Studio Developer PowerShell or another terminal where CMake can
find Visual Studio:

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug --output-on-failure
```

Release build:

```powershell
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release --output-on-failure
```

The presets build tests and the viewer. Generated files are written under
`out/`.

All third-party source subsets required for the normal build are checked in
under `third_party/`. Ordinary configure/build steps do not download
dependencies and do not depend on CMake `_deps/*-src` directories.

Generated C++ module artifacts such as BMI, IFC, PCM, and GCM files are build
outputs. They are not SDK artifacts and must not be committed.

## Run The Viewer

After a Debug build:

```powershell
.\out\build\windows-debug\bin\Debug\elf3d_viewer.exe
```

Open a model from `File > Open...`, pass a `.gltf` or `.glb` path as the first
command-line argument, or drop a model file onto the viewer window. On Windows,
the viewer is built as a GUI executable and does not open a separate console
window before the graphical window.

The build copies viewer assets beside the executable under `assets/`. Keep
`assets/font/DroidSans.ttf` and `assets/icon/*.png` with `elf3d_viewer.exe`;
they provide the Droid Sans UI font and generated toolbar icons used by the
light Low.3D-inspired viewer style.

Project-owned fixture:

```powershell
.\out\build\windows-debug\bin\Debug\elf3d_viewer.exe .\tests\fixtures\textured_pbr.gltf
```

Validate private real-file corpora without committing them:

```powershell
.\out\build\windows-debug\bin\Debug\elf3d_gltf_probe.exe `
    .\out\local-gltf-corpus
```

## Minimal Integration Example

```cpp
#include <elf3d/elf3d.h>

#include <memory>

void run_host_viewport()
{
    elf3d::EngineConfiguration configuration;
    configuration.graphics_backend = elf3d::GraphicsBackend::opengl;
    configuration.opengl.load_procedure = [](const char *name) -> elf3d::GraphicsProcedure {
        return load_host_opengl_symbol(name);
    };

    auto engine = std::move(elf3d::Engine::create(configuration)).value();
    auto scene = std::move(engine->load_scene("model.glb")).value();
    auto viewport = std::move(engine->create_viewport({1280, 720})).value();

    elf3d::EntityId camera =
        std::move(scene->create_perspective_camera({})).value();

    while (host_is_running()) {
        elf3d::ViewportInput input = translate_host_input();
        (void)viewport->update_navigation(*scene, camera, input);
        (void)viewport->render(*scene, camera);

        auto texture =
            std::move(engine->native_texture_view(viewport->color_texture()))
                .value();
        present_native_texture(texture);
    }
}
```

Production hosts should check every `Result<T>` instead of calling `value()`
directly. The host must keep a compatible OpenGL context current for viewport
creation, rendering, native texture access, and shutdown.

## Repository Structure

```text
include/elf3d/          Public C++ headers
facade/elf3d/           Public shared-library facade implementation
modules/                Internal engine modules and tests
integrations/imgui/     Optional Dear ImGui integration target
apps/viewer/            Reference viewer application
tests/                  Public API tests and fixtures
third_party/            Vendored third-party source subsets and license notices
cmake/                  Build configuration helpers
docs/                   Technical documentation and release records
```

## Documentation

- [Public API overview](docs/PUBLIC_API_OVERVIEW.md)
- [Module map](docs/MODULE_MAP.md)
- [glTF support matrix](docs/GLTF_SUPPORT.md)
- [Rendering pipeline](docs/RENDERING_PIPELINE.md)
- [Viewport and tools](docs/VIEWPORT_AND_TOOLS.md)
- [Lifetime and threading](docs/LIFETIME_AND_THREADING.md)
- [Testing](docs/TESTING.md)
- [Performance baseline](docs/PERFORMANCE_BASELINE.md)
- [Roadmap](docs/ROADMAP.md)
- [User guide](docs/USER_GUIDE.md)
- [0.7.1 local release records](docs/releases/0.7.1/)
- [Previous 0.6.0 local release records](docs/releases/0.6.0/)
- [Previous 0.4.0 release candidate records](docs/releases/0.4.0/)
- [Latest public release records](docs/releases/0.2.0/)
- [Previous 0.1.0 release records](docs/releases/0.1.0/)

## Release

The `0.7.1` release source is prepared locally. GitHub publication, CI,
GitHub Release creation, asset upload, and public clone verification are left
for manual follow-up. The latest public release remains 0.2.0, published from
tag `v0.2.0` as [Elf3D 0.2.0](https://github.com/zavelski/elf3d/releases/tag/v0.2.0).
The previous 0.1.0 release remains available from tag `v0.1.0`.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Please do not upload confidential,
customer-owned, or license-ambiguous models when reporting issues.

## License

Elf3D original source code is licensed under the MIT License. See
[LICENSE](LICENSE).

Third-party components remain governed by their own licenses and notices. See
[THIRD_PARTY.md](THIRD_PARTY.md).
