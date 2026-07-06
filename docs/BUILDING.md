# Building Elf3D

## Requirements

The supported build configuration is:

- Windows x64;
- Visual Studio 2022 with the Desktop development with C++ workload;
- CMake 3.28 or newer;
- an OpenGL 4.1-capable graphics driver.

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

Keep the generated `assets` directory beside the viewer executable when
copying it to another location.

## Main Targets

| Target | Purpose |
| --- | --- |
| `elf3d` | Shared C++ engine library |
| `elf3d_imgui` | Dear ImGui presentation integration |
| `elf3d_viewer` | Desktop reference viewer |

Set `ELF3D_BUILD_VIEWER=OFF` when only the engine library is required. Standard
CMake testing can be controlled with `BUILD_TESTING`.

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
