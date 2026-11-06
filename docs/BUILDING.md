# Building Elf3D

Use Windows x64, Visual Studio 2026/MSBuild 18.x, v145, Windows SDK
10.0.26100.0 and PowerShell 7.6.5 or newer. Root `.vsconfig` lists VS components.
Projects are checked in: no solution generation or CMake installation is needed.
Project-owned C++ uses C++20, Debug `/MDd`, Release `/MD`. Dependencies are
vendored under `third_party`; builds do not download them.

## Manual solutions

Open solutions in `win`, select the same configuration, and build in order:

| Solution | Produces |
| --- | --- |
| `Elf3D-Dependencies.slnx` | zlib, PNG, JPEG, cgltf, MikkTSpace, GLAD, GLFW |
| `Elf3D-Engine.slnx` | Model, private Runtime archive, public engine DLL |
| `Elf3D-ImGui.slnx` | Dear ImGui and GLFW/OpenGL backends |
| `Elf3D-Viewer.slnx` | Elf3D/ImGui integration, App, Viewer |
| `Elf3D-Cmd.slnx` | CPU-only command application |

Engine and ImGui can be built in either order after Dependencies. Cmd needs only
Model and CPU dependencies. Each solution builds and cleans its own projects;
Rebuild Viewer does not rebuild upstream components. Missing prerequisites name
the producer and configuration. Debug never falls back to Release.
Viewer and Cmd are the startup projects in their solutions; F5 uses matching DLLs.

## Command line

```powershell
.\tools\build.ps1 -Scope All -Configuration Debug
.\tools\build.ps1 -Scope Viewer -Configuration Release
.\tools\build.ps1 -Scope Model -Configuration Both
```

`All` builds the five production parts in dependency order. `Model` builds five
CPU dependencies, Model and Cmd, without graphics. Other scopes are Dependencies,
Engine, ImGui, Viewer, Cmd, Tests and Tools. Tests and tools are opt-in.
`-Target Build|Rebuild|Clean` selects the operation; `-Project` selects names
within a scope. The wrapper discovers VS with vswhere, without a fixed IDE path.
Compiler upgrades require rebuilding matching producers and consumers.

## Layout and outputs

Public headers are in `include/elf3d`; implementations/private headers are in
`src/<component>`. Tests and fixtures are in `tests`. Native projects in
`win/projects` are the single source lists; shared settings are in
`win/Elf3D.Build.props` and `.targets`. Root `VERSION` is the sole version setting;
version changes need no solution regeneration. MSBuild generates version-resource
headers in `out/obj`; DLL/EXE version resources and packaging read the same version.

Production EXE, DLL and LIB files are in `x64`. Debug names end in `D`, such as
`elf3dD.dll` and `elf3d_viewerD.exe`. Release names remain `elf3d.dll` and
`elf3d_viewer.exe`. Viewer assets are in `x64/assets`. Objects, PCH and other
intermediates are in `out/obj/<Configuration>/<Project>`; current product PDBs
are in `out/symbols/<Configuration>`. Tests use `out/tests/<Configuration>`;
tools use `out/tools/<Configuration>`. Test runners set DLL search paths instead
of copying product DLLs. No test output goes to `x64`.

## External applications

Import `win/Elf3D.props` after your C++ properties. Set `Elf3DSourceRoot` to the
checkout root with a trailing separator and `Elf3DComponent` to Model, Runtime
(default), or App. Use x64, C++20 and the matching `/MDd` or `/MD` runtime.
Build required producers first. The sheet exposes public headers and ready
libraries; it exports neither private include paths nor a PCH requirement.
Native standalone examples are in `examples/external_model` and
`examples/external_application`. App uses the standard graphical lifecycle;
Model requires no graphics. Public C++ include spellings are unchanged.

## Cleanup

```powershell
.\tools\clean.ps1 -WhatIf
.\tools\clean.ps1
.\tools\clean.ps1 -Scope Tests
.\tools\clean.ps1 -Scope Build -Configuration Release
```

Run cleanup while builds, tests and packaging are idle. The default `Routine`
scope is the periodic, cache-preserving command: it removes only known scratch,
public/native validation snapshots, test `run` data and package-check/staging
directories with no writes in the last seven days. `-OlderThanDays 14` changes
this interval. A newly written file keeps its whole workspace, including recent
failure evidence. Test/tool binaries and OBJ/PCH remain ready for reuse.

Successful validation and package checks already remove their heavy workspaces.
Packaging removes its staging directory after the ZIP, smoke checks and checksum
succeed; on failure it keeps staging for diagnosis. Latest success/failure
reports use fixed paths. No background service or scheduled task is required:
run the default command occasionally. It reports selected and removed sizes;
`-WhatIf` previews the same selection without changing files.

Temp immediately removes known disposable snapshots/scratch and package staging
(without the seven-day grace period). Tests removes
test/tool outputs. Build removes object/PCH caches. All combines these scopes
and preserves x64, current symbols, release ZIPs, reference data and IDE settings.
Build/All cause recompilation and are never periodic housekeeping. Use
`-Scope Releases -Keep 3 -WhatIf` to review older release ZIPs, then repeat without
`-WhatIf` to prune them explicitly (only ZIPs directly under `out/release`).
Cleanup validates the entire selection before deletion and refuses junctions,
symbolic links and nested Git repositories. Unknown directories, reference
captures, private models and resumable audit state are outside routine cleanup.
Keep one cache per configuration, with no dated build trees or source copies.
Git retains source history. Required test sources are development inputs.
