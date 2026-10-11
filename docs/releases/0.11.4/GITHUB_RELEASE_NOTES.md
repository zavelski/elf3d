# Elf3D 0.11.4

Elf3D 0.11.4 adds the standalone Cmd application for CPU-only model processing
and improves normalized glTF/GLB export and resource preservation.

## Changes

- `elf3d_cmd` rotates and exports glTF/GLB through the Model SDK. `--rotate x y z`
  applies degrees in X/Y/Z order around the world-geometry bounds center.
  Diagnostics identify import fallbacks and export fidelity limits. The
  application source can be copied into other Model SDK consumers.
- Normalized JSON uses wrapped arrays, four-group column-major matrices,
  shortest binary32 decimal tokens and safe identity omission. Cmd cleans
  near-integer orthonormal rotation coefficients within `1e-7` and reports it.
- Node-matrix edits retain owned extras and opaque extension blocks. Export
  retains material-volume factors and thickness textures plus authored required
  declarations whose payloads survive. Structural edits still invalidate raw
  metadata; arbitrary spatial metadata is not transformed or remapped.
- Perspective camera names, authored aspect ratios, infinite far planes and
  camera metadata survive round trips. Material names are retained, CMYK/YCCK
  JPEG textures decode into RGBA8, supported uncompressed RGB TIFF textures
  retain their original streams, and normal repair preserves valid directions.
- `win/cmd/Elf3D-Cmd.slnx` is the fifth independent manual Windows solution.
  Regenerate all five with `cmake/configure-win.ps1` after a version or shared
  build configuration change.

## Windows package

Download `elf3d-viewer-0.11.4-windows-x64.zip` and verify it against
`SHA256SUMS.txt`. Extract the complete archive and run `elf3d_viewer.exe` or:

```text
elf3d_cmd.exe --input model.glb --output rotated.glb --rotate 0 0 90
```

Keep `assets` beside the Viewer executable. The ZIP contains both applications,
`elf3d.dll`, guides and license notices; it is a runnable application package.

## Requirements and limitations

- Windows x64 and Microsoft Visual C++ v14 x64 Redistributable at least as recent
  as the MSVC 19.51 build tools.
- Viewer requires an OpenGL 4.1 core-profile driver; Cmd needs no graphics driver.
- Source builds use Visual Studio 2026 with v145, CMake 4.4.3 and PowerShell
  7.6.5 or newer. Other platforms remain unvalidated portability targets.
- The Model SDK is source-integrated. The C++ DLL API does not promise a stable
  ABI across toolchains, and the ZIP is not a binary C++ SDK.
- Export covers the supported static-model subset. Animation, skinning, morphs
  and arbitrary metadata remapping are outside the processing contract.
- Retaining material-volume data does not add volume rendering. TIFF support
  is limited to the documented uncompressed RGB layouts.

Elf3D source is licensed under MIT. Third-party software retains its own terms
and notices in `THIRD_PARTY.md` and `third_party_licenses`.
