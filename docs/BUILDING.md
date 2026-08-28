# Building Elf3D

## Requirements

The supported build configuration is:

- Windows x64;
- Visual Studio 2022 with the v143 Desktop development with C++ toolset;
- CMake 4.3.4;
- an OpenGL 4.1-capable graphics driver for viewer and graphics tests.

All required third-party source is included in the repository. A normal
configure and build does not download dependencies.

## Debug Build

Run these commands from the repository root:

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug --parallel
```

The Debug viewer is written to:

```text
out/build/windows-debug/bin/Debug/elf3d_viewer.exe
```

## Release Build

```powershell
cmake --preset windows-release
cmake --build --preset windows-release --parallel
```

The Release viewer is written to:

```text
out/build/windows-release/bin/Release/elf3d_viewer.exe
```

Both checked-in full presets build the Runtime SDK, Standard Application
Framework, explicit embedding integration, reference viewer, and explicitly
enable the optional `elf3d_render_benchmark` target.

Keep the generated `assets` directory beside the viewer executable when
copying it to another location.

## Model-Only Build

Use the model-only presets for `elf3d_model` and model/import/export tests. They
do not configure Scene/Assets, renderer, OpenGL, the application framework,
embedding integration, GLFW, ImGui, viewport, or viewer targets. They explicitly
disable the optional performance benchmark and
include a configured-target dependency check:

```powershell
cmake --preset windows-model-debug
cmake --build --preset windows-model-debug --parallel
ctest --preset windows-model-debug --output-on-failure
```

The static library is written under:

```text
out/build/windows-model-debug/lib/Debug/elf3d_model.lib
```

## Main Targets

| Target | Purpose |
| --- | --- |
| `elf3d_model` | Static CPU-side model library |
| `elf3d` | Shared C++ engine library |
| `elf3d_app` / `elf3d::app` | Canonical standard desktop application lifecycle |
| `elf3d_embed` / `elf3d::embed` | Explicit host-owned context and loop integration |
| `elf3d_imgui` / `elf3d::imgui` | Named Dear ImGui presentation integration |
| `elf3d_viewer` | Desktop reference viewer |
| `elf3d_public_api_examples` | Compile-checks the canonical public integration examples |
| `elf3d_render_benchmark` | Optional hidden-context rendering benchmark |
| `elf3d_render_quality_capture` | Optional reproducible PNG/metadata capture tool |

Set `ELF3D_BUILD_VIEWER=OFF` when only the SDK/framework products are required.
Set `ELF3D_BUILD_APP=OFF` or `ELF3D_BUILD_EMBED=OFF` to omit the corresponding
integration. A custom model-only configuration must disable engine,
application, embedding, viewer, and performance-benchmark targets together;
the checked-in model-only presets provide that exact mapping. The viewer
requires the application framework, and the performance benchmark requires the
embedding integration. Standard CMake testing can be controlled with
`BUILD_TESTING`. The performance benchmark defaults to `OFF`; set
`ELF3D_BUILD_PERFORMANCE_BENCHMARK=ON` to include it in a custom engine build.
The checked-in full presets already do so and therefore build both rendering
tools.

Validate all four checked-in preset option and target contracts without
compiling them:

```powershell
.\cmake\check-preset-contracts.ps1
```

The public CI runs the full and model-only Debug profiles independently.
Scheduled and manually dispatched workflows also run both Release profiles.
Every CI job pins CMake 4.3.4; older CMake releases are not a supported
compatibility target. CI limits builds to four parallel jobs for predictable
resource use on hosted runners.

Check project-owned C++ formatting with pinned `clang-format` 22.1.8:

```powershell
.\cmake\check-format.ps1
```

## Regenerating the Built-in Studio Environment

The checked-in v3 IBL is canonical generated data. Ordinary builds embed it
but do not rebake it. After an intentional change to the analytic studio
profile or baker, regenerate and verify it from a Release build:

```powershell
cmake --build --preset windows-release `
    --target elf3d_studio_environment_baker --parallel
.\out\build\windows-release\bin\Release\elf3d_studio_environment_baker.exe `
    --output .\modules\renderer\assets\studio_environment_v3.ibl
.\out\build\windows-release\bin\Release\elf3d_studio_environment_baker.exe `
    --verify .\modules\renderer\assets\studio_environment_v3.ibl
```

Commit the baker and regenerated asset together. The normal build embeds only
v3 in `elf3d.dll`; no sidecar IBL file is shipped.

## Common Problems

- If CMake cannot locate Visual Studio, run the commands from a Visual Studio
  Developer PowerShell.
- If the viewer reports an OpenGL initialization error, update the graphics
  driver and verify OpenGL 4.1 support.
- If toolbar icons or the interface font are missing, restore the generated
  `assets` directory beside `elf3d_viewer.exe`.
- Delete the affected directory below `out/build/` and configure again when
  changing Visual Studio installations or generator settings.

See `TESTING.md` for the validation commands.
