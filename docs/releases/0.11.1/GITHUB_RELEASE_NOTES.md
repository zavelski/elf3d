# Elf3D 0.11.1

Elf3D 0.11.1 improves file-path error handling and exception containment in the
Windows viewer, Runtime SDK, and CPU-only Model SDK.

## Package

- `elf3d-viewer-0.11.1-windows-x64.zip`
- `SHA256SUMS.txt`

Extract the archive and run `elf3d_viewer.exe`. Keep the complete `assets`
directory beside the executable. The package contains the viewer, engine DLL,
user guides, and license notices.

## Changes

- glTF import/export and viewer file workflows report embedded nulls and failed
  UTF-8/native path conversion as `ErrorCode::invalid_argument`. Invalid browser
  input leaves its current directory intact; invalid optional preference paths
  are ignored.
- Scene and Model SDK boundaries route escaping allocation failures through
  the existing fatal diagnostic. Document construction, mutation, and validation
  explicitly declare `noexcept`; expected failures still use Result values or
  validation diagnostics.
- Redundant exception handlers were removed from glTF helpers and navigation.
  Allocating internal Scene operations now propagate to their owning boundary.
- Isolated subprocess tests exercise allocation failure in the Model SDK and
  Debug Scene DLL. Path tests cover embedded nulls, Unicode, and Windows invalid
  UTF-8 conversion.
- Implementation files and C++ formatting were simplified without changing
  module dependencies or resource ownership.

## Requirements and limitations

- Windows x64.
- OpenGL 4.1 core-profile graphics driver.
- Microsoft Visual C++ v14 x64 Redistributable, at least as recent as MSVC 19.51.
- Source builds require Visual Studio 2026 or newer and v145 or newer. VS 2022
  and v143 are unsupported; all four Windows profiles were rebuilt and tested
  after VS 2022 removal.
- Other platforms remain unvalidated portability targets.
- Memory exhaustion is process-fatal. A standard-library operation that
  terminates internally cannot be intercepted by an outer SDK boundary and may
  terminate without the Elf3D diagnostic.
- The Model SDK is source-integrated; the C++ DLL API does not promise a stable
  cross-toolchain ABI.

## License

Elf3D source is licensed under the MIT License. Third-party software retains its
own licenses and notices in `THIRD_PARTY.md` and `third_party_licenses/`.
