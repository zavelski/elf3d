# Elf3D 0.13.1

Elf3D 0.13.1 corrects graphics-service benchmark handling on systems where an
OpenGL 4.1 context is unavailable.

## Changes

- Return the documented skip code 77 when graphics initialization, context
  availability or graphics-version support prevents the graphics-service
  benchmark from starting. Other benchmark errors remain failures.
- Preserve strict graphics validation: `tools/test.ps1 -RequireGraphics`
  rejects unavailable graphics rather than accepting a skipped scenario.

## Windows package

Extract `elf3d-viewer-0.13.1-windows-x64.zip` and keep `assets` beside
`elf3d_viewer.exe`. The archive contains Viewer, Cmd, `elf3d.dll`, guides and
license notices. Verify the archive against `SHA256SUMS.txt`.

## Requirements and compatibility

- Windows x64 and Microsoft Visual C++ v14 x64 Redistributable at least as recent
  as MSVC 19.51. Viewer requires an OpenGL 4.1 core-profile driver; Cmd needs no
  graphics driver.
- Source builds require Visual Studio 2026, v145, Windows SDK 10.0.26100.0 and
  PowerShell 7.6.5 or newer.
- Public C++ interfaces are unchanged from 0.13.0. Keep consumer sources,
  toolchain and shared CRT settings matched.

Elf3D source is licensed under MIT. Third-party software retains its own terms
and notices in `THIRD_PARTY.md` and `third_party_licenses`.
