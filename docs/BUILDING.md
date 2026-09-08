# Building Elf3D

## Requirements

The supported build configuration is:

- Windows x64;
- Visual Studio 2026 or newer with Desktop development with C++ and native MSVC
  19.50 or newer; the current presets select v145, x64 host and target tools;
- CMake 4.4.3;
- PowerShell 7.6.5 or newer for automation;
- an OpenGL 4.1-capable graphics driver for viewer and graphics tests.

All required third-party source is included in the repository. A normal
configure and build does not download dependencies.

Use standalone CMake, CTest and CPack from the same installation. The CMake
bundled with Visual Studio may be older than the required version. The current
toolchain is VS 2026 18.9.2, MSVC 19.51.36256.0, Windows SDK 10.0.26100.0,
CMake 4.4.3 and clang-format 23.1.0. Run `cmake/check-windows-environment.ps1`
to inspect discovery before configuring.

CMake generates native `.slnx` solutions. When upgrading the compiler or
generator, preserve personal debugger settings and recreate the entire build
directory, including nested solutions and compiler module artifacts. Removing
only `CMakeCache.txt` does not remove stale projects. Older Visual Studio
generators, old MSVC compilers, and v143 overrides fail explicitly. Newer
eligible generations are permitted; select their generator/toolset in a new
build directory. C++20 and the Debug `/MDd` / Release `/MD` runtime remain required.

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
embedding integration. `ELF3D_BUILD_TESTING` controls Elf3D test targets and
registration. On the first standalone configure its default follows
`BUILD_TESTING`; when included with `add_subdirectory` it defaults to `OFF`.
All four checked-in presets explicitly enable both options. As with other
CMake options, an explicit value or existing cache value takes precedence over
the initial default. The performance benchmark defaults to `OFF`; set
`ELF3D_BUILD_PERFORMANCE_BENCHMARK=ON` to include it in a custom engine build.
The checked-in full presets already do so and therefore build both rendering
tools.

Validate all four checked-in preset option, target, and IDE contracts, plus
external-application configuration, without compiling them:

```powershell
.\cmake\check-preset-contracts.ps1
```

The public CI runs the full and model-only Debug profiles independently and
builds/tests the standalone external-application example in the full Debug job.
Scheduled and manually dispatched workflows also run both Release profiles.
Every CI job pins CMake 4.4.3; older CMake releases are not a supported
compatibility target. CI limits builds to four parallel jobs for predictable
resource use on hosted runners.

Check project-owned C++ formatting with pinned `clang-format` 23.1.0:

```powershell
.\cmake\check-format.ps1
```

## Visual Studio Study and Debugging

After `cmake --preset windows-debug`, open
`out/build/windows-debug/Elf3D.slnx` in Visual Studio 2026. Select `Debug | x64`.
In a fresh standalone solution, `elf3d_viewer` is the startup project; press F5
to build and debug it. If Visual Studio has retained a different startup choice
in its local user settings, right-click `elf3d_viewer` and select **Set as
Startup Project**.

The full solution retains the product projects, nine internal module groups,
third-party libraries, tests, tools, and CMake utility projects. Public engine
headers are visible under `elf3d/include/elf3d`; a model-only solution shows its
public subset in `elf3d_model`. Project-owned source filters mirror repository
directories, keeping each `.cppm` beside its `.cpp` implementations. Headers
are explicitly assigned to targets for navigation; adding a file on disk does
not silently add new compiled sources. CMake owns these generated projects:
edit `CMakeLists.txt`, then configure again to change them.

For a smaller study view, run from the repository root:

```powershell
.\cmake\create-study-solution.ps1 -BuildDirectory .\out\build\windows-debug
```

Open the resulting `Elf3D-Study.slnf` in the same directory. It contains viewer
and the complete recursive `ProjectReference` closure, including third-party
and CMake dependencies. The script reads the existing generated solution;
it neither builds nor changes the full `.slnx`. Regenerate the filter after
changing targets/dependencies. Tests and unrelated tools remain accessible
through the full solution. The filter uses the same binaries and build tree.

The viewer debugger working directory is the directory containing its EXE.
F5 starts normally without a model argument. To study loading, set **Project
Properties > Configuration Properties > Debugging > Command Arguments** for
`Debug | x64` to the quoted absolute fixture path, for example:

```text
"C:\Projects\Elf3D\tests\fixtures\elf3d_smoke\elf3d_smoke.gltf"
```

Leave Working Directory at its generated value. The viewer build copies its
UI assets beside the EXE. See [TESTING.md](TESTING.md#visual-studio-debugging-routes)
for breakpoints, variable inspection, and DLL/PDB checks.

## External Application with Shared Elf3D Sources

`examples/external_application` is a standalone CMake project, configured
separately from the Elf3D presets. It uses only public headers, links
`elf3d::app`, and exercises a hidden application with one frame and explicit
Scene release in `stop`. The application itself does not import internal C++
modules; CMake builds those modules inside Elf3D.

Validate the example without creating an application outside this repository:

```powershell
cmake -S examples/external_application -B out/build/external-application `
    -G "Visual Studio 18 2026" -A x64 -T v145,host=x64 "-DELF3D_SOURCE_DIR=$((Get-Location).Path)"
cmake --build out/build/external-application --config Debug `
    --target elf3d_external_application --parallel 4
ctest --test-dir out/build/external-application -C Debug --output-on-failure
```

For an application rooted at `C:\Projects\MyApp`, use the example's CMake and
source files as a starting point and configure with `-S C:/Projects/MyApp
-B C:/Projects/MyApp/out/build -DELF3D_SOURCE_DIR=C:/Projects/Elf3D` (plus the same generator and
architecture). The essential connection is:

```cmake
add_subdirectory("${ELF3D_SOURCE_DIR}" "${PROJECT_BINARY_DIR}/elf3d" EXCLUDE_FROM_ALL)
target_link_libraries(my_app PRIVATE elf3d::app)
```

That application owns its repository, source, assets, tests,
startup target, and build directory. Its solution contains its own application
and the Elf3D/dependency projects needed by that application. Building the
application builds the dependencies it needs. `EXCLUDE_FROM_ALL` does not remove
dependencies or turn source targets into prebuilt libraries.

Elf3D defaults its viewer and tests to `OFF` when included as a dependency;
the parent's `BUILD_TESTING` remains independent. Explicit
`ELF3D_BUILD_VIEWER=ON` and `ELF3D_BUILD_TESTING=ON` opt back in. With
`EXCLUDE_FROM_ALL`, optional targets unrelated to the application are available
in CMake's nested `elf3d/Elf3D.slnx`; they do not appear automatically in the
parent solution or build with the application. Build those targets separately
before running the additional Elf3D tests. Other product options retain their
existing defaults.
The parent chooses the dynamic CRT (`/MDd` in Debug, `/MD` in Release). Elf3D
preserves parent output directories; absent those, it uses its own binary
directory, including for GLFW's generated projects. The example copies the
engine DLL using `$<TARGET_FILE:elf3d>` and the application's target directory,
so it does not depend on a hardcoded DLL path.

`MyApp` references the same `Elf3D` source checkout, without copying it.
Its object files, module build artifacts, libraries, DLLs, and PDBs are built
independently under `MyApp/out/build`; the existing Elf3D build is separate.
Changing a shared source file affects both consumers on their next build.
Do not edit that shared checkout concurrently from two tasks. Use a separate
checkout pinned to a revision when an application needs independent engine
versioning.

This workflow builds Elf3D from source inside the application's solution.
The viewer release ZIP contains the runnable viewer, runtime assets, and user
documentation. Use the source tree for C++ application development.

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
