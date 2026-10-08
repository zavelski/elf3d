# Elf3D 0.11.2

Elf3D 0.11.2 unifies graphical applications under the standard application
framework and introduces four independent Visual Studio solutions for manual
Windows development.

## Package

- `elf3d-viewer-0.11.2-windows-x64.zip`
- `SHA256SUMS.txt`

Extract the archive and run `elf3d_viewer.exe`. Keep the complete `assets`
directory beside the executable. The package contains the viewer, engine DLL,
user guides, and license notices.

## Changes

- Breaking: removed `elf3d_embed`, `elf3d::embed`, `EmbeddedRuntime`, its native
  presentation types, and `ELF3D_BUILD_EMBED`. Graphical applications now use
  `Application` and `run_application()` from `elf3d_app`. The framework owns the
  window, graphics context, events, rendering, presentation, and shutdown.
  CPU-only `elf3d_model` remains independent of graphics.
- Added synchronous `Viewport::read_color_pixels()` for the last rendered,
  display-transformed image. It writes RGBA8 rows from top to bottom into an
  exactly sized caller buffer without rendering the scene again.
- Added completed-frame statistics to the next application update. Frame wall
  time includes event processing through buffer swap. Hidden capture tools,
  benchmarks, and graphical integration tests use the same application lifecycle.
- Benchmark CSV format 2 identifies full-frame measurements as
  `framework_frame`; these measurements form a separate comparison baseline
  from historical complete-frame timings.
- Added `Elf3D-Dependencies.slnx`, `Elf3D-Engine.slnx`, `Elf3D-ImGui.slnx`, and
  `Elf3D-Viewer.slnx` under `win`. Each has one project set with Debug and Release
  x64 configurations and contains production targets only. Prepare them with
  `cmake/configure-win.ps1`, then build Dependencies, Engine, ImGui, and Viewer
  in that order. Downstream rebuilds consume prebuilt upstream libraries.
- Viewer builds deploy the matching engine DLL, assets, and available symbols
  beside the executable, including an updated DLL when no relink is needed.
- Consolidated automated configuration into `windows-full` and `windows-model`
  trees. Existing configuration-specific build and test preset names remain.
  Removed the Study solution-filter workflow.

## Requirements and limitations

- Windows x64.
- OpenGL 4.1 core-profile graphics driver.
- Microsoft Visual C++ v14 x64 Redistributable, at least as recent as MSVC 19.51.
- Source builds use Visual Studio 2026 with v145, CMake 4.4.3, and PowerShell
  7.6.5 or newer. VS 2022 and v143 are unsupported.
- Other platforms remain unvalidated portability targets.
- The Model SDK is source-integrated; the C++ DLL API does not promise a stable
  cross-toolchain ABI. The viewer ZIP is not a binary SDK.
- Command-line build and graphics smoke validation do not establish Visual
  Studio F5, breakpoint, or interactive navigation behavior. Those manual
  checks for the new solutions remain unconfirmed.

## License

Elf3D source is licensed under the MIT License. Third-party software retains its
own licenses and notices in `THIRD_PARTY.md` and `third_party_licenses/`.
