# Elf3D 0.10.7

Elf3D 0.10.7 improves source navigation and debugging in Visual Studio and
source integration for external applications. The engine retains its C++20
modules, nine internal build groups, and public C++ interface.

## Package

- `elf3d-viewer-0.10.7-windows-x64.zip`
- `SHA256SUMS.txt`

The viewer ZIP contains the Windows application, engine DLL, UI assets, user
documentation, and license notices. It is a runnable viewer package, not a
prebuilt C++ SDK. The build improvements and standalone application example
are part of the source tree.

## Changes

- Project headers are visible in their owning Visual Studio projects. Source
  filters follow repository directories, keeping each module interface beside
  its implementation files.
- Standalone Elf3D solutions select `elf3d_viewer` as the initial startup
  project and use the executable directory as the debugger working directory.
  Normal startup still takes no model argument.
- `cmake/create-study-solution.ps1 -BuildDirectory <directory>` creates an
  `Elf3D-Study.slnf` from the viewer's generated project references without
  building or modifying the full solution.
- `ELF3D_BUILD_TESTING` controls Elf3D test targets and registration. It follows
  `BUILD_TESTING` on an initial standalone configure and defaults to `OFF`
  when Elf3D is included as a dependency. All four existing presets keep
  testing enabled. Explicit values take precedence over initial defaults.
- The viewer also defaults to `OFF` for dependency builds. Parent startup,
  testing, and output-directory choices are preserved, and local maintenance
  configuration remains restricted to standalone Elf3D builds.
- `examples/external_application` demonstrates shared-source integration
  through `elf3d::app`, public headers, an independent build directory, one
  hidden application frame, and explicit Scene release during shutdown.
- Windows Debug CI builds and tests the external example independently.
  Preset checks also cover IDE metadata, study-filter dependencies, and
  external-application build settings. Build and test guides include four
  breakpoint routes and DLL/PDB inspection instructions.

## Compatibility

The public C++ API and module interfaces are unchanged. This release adds a
supported CMake option, not a new SDK packaging format or an ABI guarantee.
Clients should use the same compiler toolchain and dynamic runtime
configuration as Elf3D: `/MDd` for Debug and `/MD` for Release. Existing cached
option values remain in effect until explicitly changed.

## Requirements

- Windows x64.
- OpenGL 4.1 core-profile graphics driver.
- Microsoft Visual C++ Redistributable for Visual Studio 2022.
- Source builds: Visual Studio 2022 v143 and CMake 4.3.4.

## License

Elf3D source is available under the MIT License. Third-party software and
visual subjects retain the licenses and notices included in the source tree
and viewer package.
