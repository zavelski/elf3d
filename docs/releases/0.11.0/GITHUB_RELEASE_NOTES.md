# Elf3D 0.11.0

Elf3D 0.11.0 moves Windows source builds to Visual Studio 2026 and native
CMake-generated XML solutions.

## Changes

- All four Windows presets use Visual Studio 2026, v145, and x64 host/target
  tools. Source builds require native MSVC 19.50 or newer and CMake 4.4.3.
- CMake generates `.slnx` solutions, including nested dependency solutions.
  `Elf3D-Study.slnf` retains the viewer's exact project-reference dependency
  closure and references the full solution without modifying it.
- Build checks verify startup metadata, compiler/MSBuild selection, platform,
  CRT, source organization, external-application defaults and repeated
  configuration. Unsupported compiler and toolset selections fail explicitly.
- Fixed declaration visibility and include/import ordering exposed by the new
  compiler while preserving C++20, the module graph and public C++ API.
- Windows CI uses the Visual Studio 2026 runner. Formatting uses clang-format
  23.1.0, and automation uses PowerShell 7.6.5 or newer.

## Compatibility

Recreate source build directories after upgrading. Compiler-specific module
artifacts and binaries from previous toolchains must be rebuilt. Debug uses
`/MDd` and Release uses `/MD`. The public C++ interface is unchanged; binaries
remain toolchain-sensitive. External applications keep their own tests,
startup target, and build outputs.

## Viewer requirements

- Windows x64 and an OpenGL 4.1 core-profile graphics driver.
- Microsoft Visual C++ v14 x64 Redistributable at least as recent as the
  MSVC 19.51 build tools used for this release.
- The prebuilt viewer does not require Visual Studio.

The download includes the viewer ZIP and `SHA256SUMS.txt`. Keep its assets
directory beside the executable. Source and third-party license notices are
included under their respective terms.
