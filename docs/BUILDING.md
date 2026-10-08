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
bundled with Visual Studio may be older than the required version. Run
`cmake/check-windows-environment.ps1` to inspect discovery before configuring.

CMake generates native `.slnx` solutions. When upgrading the compiler or
generator, preserve personal debugger settings and recreate the entire build
directory, including nested solutions and compiler module artifacts. Removing
only `CMakeCache.txt` does not remove stale projects. Older Visual Studio
generators, old MSVC compilers, and v143 overrides fail explicitly. A future
toolchain upgrade replaces the active baseline and requires regenerating all
build trees. C++20 and the Debug `/MDd` / Release `/MD` runtime remain required.

## Manual Visual Studio Solutions

Prepare the manual workspace from the repository root, using PowerShell 7.6.5
or newer:

```powershell
.\cmake\configure-win.ps1
```

This discovers the active VS 2026 toolchain and generates all four solutions,
including both configurations, without compiling or launching Visual Studio.
Repeat this command after changing the project version or shared build
configuration so all components have matching import metadata. Reload open
solutions in Visual Studio after regeneration. Existing binaries and personal
IDE settings are preserved.

The generated workspace is ignored by Git. Open and build these solutions in
order, selecting the **same** `Debug | x64` or `Release | x64` in each:

| Order | Solution | Production projects |
| --- | --- | ---: |
| 1 | `win/dependencies/Elf3D-Dependencies.slnx` | 7 |
| 2 | `win/engine/Elf3D-Engine.slnx` | 11 |
| 3 | `win/imgui/Elf3D-ImGui.slnx` | 2 |
| 4 | `win/viewer/Elf3D-Viewer.slnx` | 2 |

There is one project set per component. Switch between Debug and Release in
VS; a second configure tree is unnecessary. `ALL_BUILD` and `ZERO_CHECK` are
standard CMake utilities and are counted separately. Tests, examples, developer
tools, the environment baker, `RUN_TESTS`, and Dashboard projects are absent.
No solution filter is needed.

Dependencies contains zlib, PNG, JPEG, cgltf, MikkTSpace, GLAD, and GLFW. GLM is
header-only. Engine contains `elf3d`, `elf3d_model`, and the existing nine
internal module-group projects (18 C++20 modules). ImGui contains Dear ImGui
and `elf3d_imgui`. Viewer contains `elf3d_app` and `elf3d_viewer`.

Dear ImGui is independent of Engine. Our `elf3d_imgui` integration uses Elf3D
results/errors, mathematical values, diagnostics, and the private native texture
presentation bridge, so it depends on Engine. Engine does not depend on ImGui
or GLFW. `elf3d_model` remains independent of graphics.

Each component builds only its own sources. It imports upstream libraries from
this manual workspace. **Rebuild Solution in Viewer does not rebuild Engine,
ImGui, or Dependencies.** Clean affects only that component's outputs. After
changing upstream code, rebuild affected components in dependency order. After
changing public headers, also rebuild affected downstream consumers. A missing
prerequisite reports its component, configuration, path, and solution to build;
Debug never falls back to Release.

Outputs are `bin/Debug`, `bin/Release`, `lib/Debug`, and `lib/Release` inside each
component. Objects and module artifacts remain local and configuration-specific.
The runnable viewer paths are:

```text
win/viewer/bin/Debug/elf3d_viewer.exe
win/viewer/bin/Release/elf3d_viewer.exe
```

The Viewer build deploys the matching `elf3d.dll`, assets, and Engine Debug PDB
beside the EXE. Release symbols are copied when produced. File dependencies
refresh a changed Engine DLL on the next Viewer build even without relinking
the viewer. Keep the DLL and `assets` directory with a copied EXE.

In the Viewer solution, `elf3d_viewer` is the initial startup project. Select
Debug or Release and press F5 after building the upstream solutions. The
debugger working directory follows the selected EXE directory; no model
argument is required. A saved VS startup preference can override the generated
default: right-click `elf3d_viewer` and select **Set as Startup Project**.
Debug symbols allow breakpoints in both Viewer/framework and Engine code.
See [TESTING.md](TESTING.md#visual-studio-debugging-routes) for debugging routes.

Public headers and implementation files remain grouped by repository directory.
CMake owns generated projects: edit CMake source lists and regenerate rather
than editing `.vcxproj` or `.slnx` files. The private import metadata lives in
each producer's `local/` directory. It uses native configuration-aware CMake
imported targets, including separate DLL/import-library paths, shared public
usage requirements and static-link closures. Engine OBJECT targets and internal headers
artifacts do not cross this boundary. Metadata rejects incompatible source
roots, component roots, architectures, or toolchain identities. It is a local
build mechanism, not an installed binary SDK or package-registry workflow.

When upgrading the active Visual Studio toolchain, preserve personal settings,
recreate all four component directories, rerun preparation, and rebuild in
order. Old compiler/module caches must not be reused. Nested vendored CMake
solutions are implementation artifacts; the four paths above are the manual
entry points.

## Automated Builds

Build and test validation uses separate source-integrated trees and never
consumes, configures, cleans, or writes the manual `win` workspace. Full Debug
and Release share one configure tree:

```powershell
cmake --preset windows-full
cmake --build --preset windows-debug --parallel 4
ctest --preset windows-debug --output-on-failure
cmake --build --preset windows-release --parallel 4
ctest --preset windows-release --output-on-failure
```

Outputs are in `out/build/windows-full/bin/<Configuration>` and
`out/build/windows-full/lib/<Configuration>`. The full profile retains tests,
compile-checked examples, tools, and source-built dependencies.

Independent CPU/model validation uses a second tree:

```powershell
cmake --preset windows-model
cmake --build --preset windows-model-debug --parallel 4
ctest --preset windows-model-debug --output-on-failure
cmake --build --preset windows-model-release --parallel 4
ctest --preset windows-model-release --output-on-failure
```

Its outputs are under `out/build/windows-model`. It does not configure Scene,
renderer, OpenGL, GLFW, ImGui, framework, or Viewer. This independent tree proves
the CPU library does not acquire a graphics dependency.

The four build/test preset names remain unchanged. Only configure preset names
and their build-directory mappings changed. The common hidden preset owns the
VS generator, platform, toolset, and two configurations. `ELF3D_BUILD_COMPONENT`
is a private selector: `all` (normal source integration), `dependencies`,
`engine`, `imgui`, or `viewer`. Component modes are top-level workflows and
select their source targets before testing infrastructure is instantiated.
Normal `add_subdirectory` integration remains source-built.

Configure/IDE contracts, including all four component compositions:

```powershell
.\cmake\check-preset-contracts.ps1
```

Complete component build/ownership/import/runtime checks in an isolated tree:

```powershell
.\cmake\check-win-components.ps1 -Build
```

The checker retains its logs below `out`. `configure-win.ps1 -BuildRoot
<path-under-out>` also supports isolated preparation without touching `win`.
Project-owned C++ formatting uses `cmake/check-format.ps1` and pinned
clang-format 23.1.0. Full/model-only Debug run in independent CI jobs; scheduled
or manually dispatched CI additionally tests Release. Graphics initialization
skips are reported separately from successful rendering validation.

## External Application with Shared Elf3D Sources

`examples/external_application` is a standalone CMake project, configured
separately from the Elf3D presets. It uses only public headers, links
`elf3d::app`, and exercises a hidden application with one frame and explicit
Scene release in `stop`. The application uses public SDK/integration headers;
CMake builds the conventional engine component sources inside Elf3D.

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
Its object files, private PCH artifacts, libraries, DLLs, and PDBs are built
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
.\out\build\windows-full\bin\Release\elf3d_studio_environment_baker.exe `
    --output .\modules\renderer\assets\studio_environment_v3.ibl
.\out\build\windows-full\bin\Release\elf3d_studio_environment_baker.exe `
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

## Header and PCH Build Contract

First-party code uses C++20 conventional headers and sources. C++ Modules
are prohibited by `CODING_POLICY.md`. Windows production targets use private
CMake-generated PCH; public consumers inherit no PCH requirement or internal
include directory. Other platforms keep PCH disabled.

Use `-DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON` in a separate configure tree to
validate without PCH. Do not edit generated Visual Studio project files. After
shared configuration changes, regenerate all four manual solutions with
`pwsh -NoProfile -File .\cmake\configure-win.ps1`; preserve existing manual
binaries and IDE settings and validate builds under `out`.
