# Elf3D 0.11.3

Elf3D 0.11.3 replaces first-party C++ Modules with conventional headers and
source files and introduces private precompiled headers for Windows production
builds. Public APIs and viewer behavior are preserved.

## Package

- `elf3d-viewer-0.11.3-windows-x64.zip`
- `SHA256SUMS.txt`

Extract the archive and run `elf3d_viewer.exe`. Keep the complete `assets`
directory beside the executable. The package contains the viewer, engine DLL,
user guides, and license notices.

## Changes

- All first-party code uses C++20 conventional headers and explicit includes.
  The 18 architectural components, their dependency layers, and the nine
  internal OBJECT-library groups retain their existing responsibilities.
- Core and Model reuse their canonical SDK headers. Other internal contracts
  remain private to their owning components; public consumers receive only
  supported SDK and integration headers.
- Windows production targets use private CMake-generated PCH with separate
  profiles for the engine/framework, viewer, ImGui, and ImGui integration.
  PCH contains stable standard-library headers and does not hide required
  includes. Use `-DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON` to build without PCH.
  Other platforms keep PCH disabled by default.
- Source and build checks enforce the C++ Modules prohibition and validate
  component include dependencies, layers, cycles, private-header access, and
  agreement with CMake groups. Header contracts compile independently and with
  repeated inclusion; combined translation units check link conflicts.
- The four independent manual Windows solutions remain the supported manual
  workspace. Regenerate all four with `cmake/configure-win.ps1` after changing
  the project version or shared build configuration. Regeneration preserves
  existing manual outputs and personal IDE settings.

## Requirements and limitations

- Windows x64.
- OpenGL 4.1 core-profile graphics driver.
- Microsoft Visual C++ v14 x64 Redistributable, at least as recent as MSVC 19.51.
- Source builds use Visual Studio 2026 with v145, CMake 4.4.3, and PowerShell
  7.6.5 or newer. VS 2022 and v143 are unsupported.
- Other platforms remain unvalidated portability targets. GCC 13.3 header
  checks do not establish full Linux build or runtime validation.
- The Model SDK is source-integrated; the C++ DLL API does not promise a stable
  cross-toolchain ABI. The viewer ZIP is not a binary SDK.
- Automated graphics tests do not establish visible viewer interaction or
  Visual Studio F5 and breakpoint behavior. Those manual checks remain separate.
- This source migration does not claim improved clean-build performance over
  the previous module implementation.

## License

Elf3D source is licensed under the MIT License. Third-party software retains its
own licenses and notices in `THIRD_PARTY.md` and `third_party_licenses/`.
