<div align="center">

# Elf3D

**A focused C++20 3D visualization engine and Windows viewer for glTF 2.0.**

[![CI](https://github.com/zavelski/elf3d/actions/workflows/ci.yml/badge.svg)](https://github.com/zavelski/elf3d/actions/workflows/ci.yml)
[![Latest release](https://img.shields.io/github/v/release/zavelski/elf3d?display_name=tag&sort=semver)](https://github.com/zavelski/elf3d/releases/latest)
[![License](https://img.shields.io/github/license/zavelski/elf3d)](LICENSE)
![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C)
![Platform](https://img.shields.io/badge/validated-Windows%20x64-0078D4)

[Download the viewer](https://github.com/zavelski/elf3d/releases/latest)
· [Viewer guide](docs/GUIDE.md)
· [C++ API](docs/PUBLIC_API.md)
· [glTF compatibility](docs/GLTF.md)

</div>

![Elf3D Viewer inspecting the Madame Walker Theatre scene](docs/assets/readme/elf3d-viewer-madame-walker-theatre.png)

<div align="center">
<sub>
Scene: <a href="https://sketchfab.com/3d-models/madame-walker-theatre-98ba4154bbb644bb9cb4d9c68d7dd87b">Madame Walker Theatre</a>
by <a href="https://sketchfab.com/iupuiul">IUPUI University Library</a>,
licensed under <a href="https://creativecommons.org/licenses/by/4.0/">CC BY 4.0</a>.
</sub>
</div>

## What is Elf3D?

Elf3D is both a ready-to-run model inspector and an embeddable rendering stack.
Open a `.gltf` or `.glb`, explore its hierarchy and materials, select or hide
objects, measure surfaces, and cut through geometry with section planes and
clipping boxes.

The project is deliberately focused. It is a static-scene visualization engine,
not a game engine or a general-purpose content editor. The standard application
framework owns the native window, event loop, OpenGL context, normalized input,
frame sequence, final presentation, and teardown. Applications own composition
and concrete Components, Tools, and Workflows. Hosts that need to retain their
own window and loop use the separately named embedding integration.

## Highlights

- **Model-first workflow** — load and save `.gltf`/`.glb`, retain every scene,
  inspect canonical `elf3d::Document` data, validate references, replace
  primitives, and preserve safe source image and JSON metadata.
- **Interactive inspection** — hierarchy browsing, orbit/pan/dolly navigation,
  GPU-assisted picking, selection, visibility, isolation, visible bounds, and
  model statistics.
- **Analysis tools** — point-to-point surface measurement, one section plane,
  up to three clipping boxes, and backend-neutral helper overlays.
- **OpenGL 4.1 rendering** — metallic/roughness material values, base-color,
  emissive and occlusion textures, vertex color, unlit materials, alpha mask
  and blend paths, and off-screen viewport output.
- **Two explicit lifecycle products** — `elf3d_app` provides the canonical
  Elf3D-owned desktop lifecycle, while `elf3d_embed` supports host-owned window,
  context, input, presentation, and teardown. Neither leaks GLFW, Dear ImGui,
  OpenGL, GLM, or cgltf types through the Runtime SDK API.
- **Bounded input handling** — structured compatibility diagnostics and
  reviewed limits for files, buffers, images, hierarchy depth, and geometry.

## Choose your entry point

| Target | Use it when you need |
| --- | --- |
| `elf3d_viewer` | A Windows desktop application for opening, inspecting, and exporting models. |
| `elf3d_app` / `elf3d::app` | The canonical desktop lifecycle with normalized input and queued viewport rendering. |
| `elf3d_embed` / `elf3d::embed` | An explicit host-owned window, context, loop, and presentation path. |
| `elf3d` / `elf3d::elf3d` | The shared Runtime SDK: Scene, Viewport, rendering, picking, navigation, and general mechanisms. |
| `elf3d_model` / `elf3d::model` | A static CPU-only `Document` library for construction, validation, processing, and glTF/GLB import/export. |
| `elf3d_imgui` / `elf3d::imgui` | Named Dear ImGui presentation integration used by the standard desktop framework. |

## Download the viewer

Download the current
[Elf3D 0.10.3 Windows x64 package](https://github.com/zavelski/elf3d/releases/tag/v0.10.3),
extract it, and run `elf3d_viewer.exe`.

Requirements:

- Windows x64;
- an OpenGL 4.1-capable graphics driver;
- Microsoft Visual C++ Redistributable for Visual Studio 2022.

Open a model from **File > Open...**, pass its path as the first command-line
argument, or drop it onto the viewer. Use **File > Save As...** to export the
retained model as `.gltf` or `.glb`.

## Build from source

Install Visual Studio 2022 with the v143 Desktop development with C++ toolset
and CMake 4.3.4. From the repository root:

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug --parallel
ctest --preset windows-debug --output-on-failure
```

Run the viewer:

```powershell
.\out\build\windows-debug\bin\Debug\elf3d_viewer.exe
```

For the model-only static library and tests:

```powershell
cmake --preset windows-model-debug
cmake --build --preset windows-model-debug --parallel
ctest --preset windows-model-debug --output-on-failure
```

Release configurations, output paths, and troubleshooting are documented in
[Building Elf3D](docs/BUILDING.md).

## Architecture

Elf3D uses 18 restricted C++20 named modules as its primary architecture.
Nine internal CMake `OBJECT` targets group those modules for practical build
and IDE scale; the groups do not replace the module boundaries.

```mermaid
flowchart TD
    Viewer["elf3d_viewer<br/>Components · Tools · Workflows"] --> App["elf3d_app<br/>standard lifecycle"]
    Viewer --> ImGui["elf3d_imgui<br/>named UI integration"]
    Viewer --> Engine["elf3d<br/>Runtime SDK"]
    App --> ImGui
    App --> Engine
    Embedder["External host"] --> Embed["elf3d_embed<br/>explicit embedding path"]
    Embed --> Engine

    ModelProduct["elf3d_model<br/>static model library"] --> Foundation["elf3d_foundation_modules<br/>core · math"]
    ModelProduct --> Image["elf3d_image_modules<br/>PNG · JPEG boundary"]
    ModelProduct --> Model["elf3d_model_modules<br/>Document"]
    ModelProduct --> Gltf["elf3d_gltf_modules<br/>glTF · GLB"]

    Engine --> Foundation
    Engine --> Domain["elf3d_domain_modules<br/>interaction · assets · clipping · scene"]
    Engine --> Image
    Engine --> Model
    Engine --> Gltf
    Engine --> Graphics["elf3d_graphics_modules<br/>backend-neutral graphics"]
    Engine --> OpenGL["elf3d_opengl_modules<br/>OpenGL 4.1 backend"]
    Engine --> Interaction["elf3d_interaction_modules<br/>navigation · picking · view mechanisms"]
    Engine --> View["elf3d_view_modules<br/>renderer · viewport"]

    View --> Interaction
    View --> Graphics
    Interaction --> Domain
    OpenGL --> Graphics
    Graphics --> Domain
    Domain --> Model
    Domain --> Foundation
    Gltf --> Image
    Gltf --> Model
    Image --> Foundation
    Model --> Foundation
```

The dependency direction is intentionally one-way:

- engine and domain modules do not depend on Dear ImGui, GLFW, or application
  GUI code;
- Scene remains independent of Renderer and concrete graphics backends;
- native OpenGL and third-party types stay inside named boundary adapters;
- `elf3d_model` configures without Scene, Renderer, OpenGL, ImGui, GLFW, or the
  viewer.

See the [C++ API guide](docs/PUBLIC_API.md) for ownership and shutdown rules.

## Current scope

Elf3D concentrates on static glTF inspection. The supported path includes all
scenes, perspective cameras, indexed and non-indexed triangle geometry,
triangle strip/fan conversion, two UV sets, vertex color, core PBR values,
selected material extensions, PNG/JPEG images, hierarchy, transforms, model
diagnostics, and transactional export.

Animation playback, skinning, morph deformation, orthographic rendering,
authored scene lights, shadows, external HDR environments and skyboxes,
automatic exposure, tangent-space normal mapping, compressed geometry,
KTX2/BasisU/WebP, and order-independent transparency are outside the current
rendering scope. Standard PBR does include a built-in neutral studio
image-based-lighting profile and PBR Neutral tone mapping. Windows x64 is the
validated platform; other platforms remain portability targets.

The detailed support matrix is in [glTF compatibility](docs/GLTF.md), and the
graphics behavior is in [Rendering reference](docs/RENDERING.md).

## Repository map

| Path | Responsibility |
| --- | --- |
| `include/elf3d/` | Public C++ headers |
| `facade/elf3d/` | Shared-library entry points and public/internal conversion |
| `modules/` | Named modules, implementations, and focused tests |
| `framework/app/` | Standard desktop application lifecycle and normalized input |
| `integrations/embed/` | Explicit host-owned context and loop integration |
| `integrations/imgui/` | Named Dear ImGui presentation integration |
| `apps/viewer/` | Reference application assembly and runtime assets |
| `examples/` | Compile-checked public integration examples |
| `tests/` | Public API, external copy/adapt, and real OpenGL integration tests |
| `cmake/` | Target-scoped build configuration |
| `third_party/` | Pinned vendored dependencies and license notices |
| `docs/` | Viewer, API, format, rendering, build, and testing documentation |

## Documentation

- [Practical viewer guide](docs/GUIDE.md)
- [Viewer controls and reference](docs/VIEWER.md)
- [C++ API guide](docs/PUBLIC_API.md)
- [glTF compatibility](docs/GLTF.md)
- [Rendering reference](docs/RENDERING.md)
- [Building](docs/BUILDING.md)
- [Testing](docs/TESTING.md)
- [Support](SUPPORT.md)

## Contributing

Bug reports and focused contributions are welcome. See
[CONTRIBUTING.md](CONTRIBUTING.md), and do not upload confidential,
customer-owned, or license-restricted models with an issue.

## License

Elf3D source is available under the [MIT License](LICENSE). Vendored
dependencies, runtime assets, and README visuals retain their respective
licenses and notices; see [THIRD_PARTY.md](THIRD_PARTY.md) and the
[README visual attribution](docs/assets/readme/ATTRIBUTION.md).
