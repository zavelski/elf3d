# Elf3D 0.10.3

Elf3D 0.10.3 adds a neutral studio rendering profile for more balanced scene
inspection. It keeps surfaces readable across view directions, restores shaped
reflections on glossy and metallic materials, and preserves detail in bright
regions through a neutral display transform.

## Download

- `elf3d-viewer-0.10.3-windows-x64.zip`
- `SHA256SUMS.txt`

The viewer package is a ready-to-run Windows application. GitHub also provides
source archives for the `v0.10.3` tag.

## Highlights

- Adds renderer-owned studio image-based lighting with diffuse irradiance,
  roughness-prefiltered specular reflections, and a BRDF integration lookup.
- Adds fixed exposure and Khronos PBR Neutral tone mapping to preserve bright
  gradation without automatic exposure shifts.
- Adds Viewer controls for environment intensity and rotation, exposure, tone
  mapping, legacy ambient fill, and exact lighting reset defaults.
- Adds Camera Evidence diagnostics and reproducible hidden-context PNG and
  metadata capture tooling for rendering comparisons.
- Expands deterministic rendering tests for display mapping, glossy and rough
  materials, OpenGL state restoration, resource lifetime, and calibrated pixel
  invariants.

## Compatibility

This release adds public viewport environment-lighting and display-transform
settings. The default Standard PBR presentation now uses the neutral studio
environment, directional intensity 2.0, legacy ambient 0.0, exposure 0 EV, and
PBR Neutral tone mapping. Unlit remains available as a diagnostic mode.

The C++ interface remains toolchain-sensitive rather than a stable binary ABI;
clients should rebuild with the same compiler toolchain and runtime
configuration as Elf3D.

## Requirements

- Windows x64.
- OpenGL 4.1 core-profile graphics driver.
- Microsoft Visual C++ Redistributable for Visual Studio 2022.

## License

Elf3D source is available under the MIT License. Third-party software and
visual subjects retain the licenses and notices included in the source tree
and viewer package.
