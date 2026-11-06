# Elf3D 0.12.1

Elf3D 0.12.1 moves the supported Windows source build to native Visual Studio
2026 projects and simplifies the source and output layout.

## Changes

- Build directly with the checked-in `.slnx` and `.vcxproj` files. CMake, CTest
  and CPack are no longer required for supported Windows builds.
- Keep five independent production solutions in `win`: Dependencies, Engine,
  ImGui, Viewer and Cmd. Viewer Clean/Rebuild preserves upstream components;
  missing or incompatible prerequisites identify the required producer.
- Consolidate implementation in `src`, public SDK/integration headers in
  `include/elf3d`, and tests in `tests`. Existing public include spellings and
  the 18 logical component boundaries are retained.
- Group Engine compilation into Model and private Runtime archives plus the
  public facade DLL. Model/Cmd retain their CPU-only build path.
- Place production EXE, DLL and LIB files in `x64`. Debug outputs use a `D`
  suffix; tests, developer tools, objects and generated files stay under `out`.
- Use `tools/build.ps1`, `tools/test.ps1` and `tools/clean.ps1` for automation.
  Tests and developer tools have separate solutions. Cleanup supports previews
  and preserves current products, symbols, archives, references and IDE settings.
- Import `win/Elf3D.props` in external MSBuild consumers and select Model,
  Runtime or App. Public release packaging verifies audited source identity,
  compiler/configuration provenance and hashes of the packaged outputs.

## Windows package

Extract `elf3d-viewer-0.12.1-windows-x64.zip` and keep `assets` beside
`elf3d_viewer.exe`. The archive contains Viewer, Cmd, `elf3d.dll`, guides and
license notices. Verify the archive against `SHA256SUMS.txt`.

```text
elf3d_cmd.exe --input model.glb --output rotated.glb --rotate 0 0 90
```

## Requirements and compatibility

- Windows x64 and Microsoft Visual C++ v14 x64 Redistributable at least as recent
  as MSVC 19.51. Viewer requires an OpenGL 4.1 core-profile driver; Cmd needs no
  graphics driver.
- Source builds require Visual Studio 2026, v145, Windows SDK 10.0.26100.0 and
  PowerShell 7.6.5 or newer. Build Dependencies first, then Engine and ImGui,
  then Viewer. Use the same Debug/Release configuration throughout.
- CMake integration is no longer maintained. External source consumers use
  native MSBuild integration. C++ consumers require matching sources, toolchain
  and shared CRT settings; the ZIP is a runnable application package rather
  than a binary C++ SDK.
- Runtime functionality and public C++ API declarations are retained from
  0.12.0. Other platforms remain unvalidated portability targets.

Elf3D source is licensed under MIT. Third-party software retains its own terms
and notices in `THIRD_PARTY.md` and `third_party_licenses`.
