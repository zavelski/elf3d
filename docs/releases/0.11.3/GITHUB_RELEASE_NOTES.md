# Elf3D 0.11.3

Elf3D 0.11.3 replaces first-party C++ Modules with conventional headers and
source files and introduces private precompiled headers for Windows production
builds. Public APIs and viewer behavior are preserved.

## Package

- `elf3d-viewer-0.11.3-windows-x64.zip`
- `SHA256SUMS.txt`

Extract the archive and run `elf3d_viewer.exe`. Keep the complete `assets`
directory beside the executable. The package includes `elf3d_cmd.exe`,
`docs/CMD.md`, the engine DLL, user guides, and license notices.

## Changes

- Added `elf3d_cmd` for CPU-only glTF/GLB rotation and export through the public
  Model SDK. `--rotate x y z` uses degrees in X/Y/Z order around the world-geometry
  bounds center. Its source can be copied into external applications.
- Added `win/cmd/Elf3D-Cmd.slnx` as the fifth independent manual solution.
- Normalize glTF/GLB JSON, including retained metadata, with wrapped arrays,
  four-group column-major matrices, shortest float tokens, and safe identity
  omission. Cmd enables bounded orthonormal matrix-basis cleanup with diagnostics.
- Unicode glTF filenames retain UTF-8 sidecar URIs and native transaction paths.

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
- The five independent manual Windows solutions remain the supported manual
  workspace. Regenerate all five with `cmake/configure-win.ps1` after changing
  the project version or shared build configuration. Regeneration preserves
  existing manual outputs and personal IDE settings.

## Requirements and limitations

- Windows x64.
- OpenGL 4.1 core-profile graphics driver for Viewer; Cmd needs no graphics driver.
- Microsoft Visual C++ v14 x64 Redistributable, at least as recent as MSVC 19.51.
- Source builds use Visual Studio 2026 with v145, CMake 4.4.3, and PowerShell
  7.6.5 or newer. VS 2022 and v143 are unsupported.
- Other platforms remain unvalidated portability targets. GCC 13.3 header
  checks do not establish full Linux build or runtime validation.
- The Model SDK is source-integrated; the C++ DLL API does not promise a stable
  cross-toolchain ABI. The viewer ZIP is not a binary SDK.
- Automated graphics tests do not establish visible viewer interaction or
  Visual Studio F5 and breakpoint behavior. Those manual checks remain separate.
- Cmd exports the supported static-model subset. Effective mutation drops raw
  `extras` and unknown extensions with a warning; animation, skin, morph, and
  semantic metadata remapping are outside this release's processing contract.
- This source migration does not claim improved clean-build performance over
  the previous module implementation.

## License

Elf3D source is licensed under the MIT License. Third-party software retains its
own licenses and notices in `THIRD_PARTY.md` and `third_party_licenses/`.

The Cmd rotation path now retains owned extras and opaque extension blocks on
supported Document objects. The Model SDK represents material-volume factors
and thickness textures for normalized export, and retains authored required
extension declarations while their payloads survive. Opaque spatial semantics
are not interpreted; structural edits still invalidate raw metadata. Volume
retention does not implement volume rendering or arbitrary glTF losslessness.

Normalized Model SDK export now retains camera metadata. Authored perspective camera
properties and material names survive round trips. CMYK/YCCK JPEG textures are
supported, and invalid-normal repair preserves valid authored normal directions.
These changes do not add camera IBL, EXR decoding, or volume effects to rendering.

The Model SDK preserves and decodes supported uncompressed RGB TIFF textures,
retaining original 16-bit sample streams on unchanged export. Other TIFF layouts
remain outside the supported subset and produce diagnostics.
