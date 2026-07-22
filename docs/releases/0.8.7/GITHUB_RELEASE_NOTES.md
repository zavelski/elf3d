# Elf3D 0.8.7

Elf3D 0.8.7 substantially reduces large-scene rendering overhead and adds
repeatable diagnostics while preserving the existing OpenGL 4.1, standard PBR,
full-resolution viewer defaults.

## Download

- `elf3d-viewer-0.8.7-windows-x64.zip`
- `SHA256SUMS.txt`

The viewer package is a ready-to-run Windows application. GitHub also provides
source archives for the `v0.8.7` tag.

## Highlights

- Batches OpenGL state containment and draw submission at pass boundaries,
  eliminating per-primitive context, state snapshot, restore, and error-query
  overhead from Release rendering.
- Replaces linear renderer resource searches with indexed caches and reusable
  immutable draw packets. Camera movement no longer rebuilds static packets or
  uploads unchanged geometry.
- Caches world transforms, primitive bounds, and visible bounds, then rejects
  fully outside geometry against the camera frustum before draw preparation.
- Selects 24-byte position/normal and 32-byte single-UV vertex layouts when
  wider 56-byte vertex storage is unnecessary.
- Keeps focus-depth and ID picking targets independent, preventing orbit entry
  from resizing the selection target and limiting navigation to one focus
  request per initiation.
- Reuses the last resolved 3D texture while the viewport is idle, while input,
  the interface, and window presentation continue each host frame.
- Adds delayed nonblocking GPU timers, detailed CPU/pass/resource/residency
  statistics, viewer CSV capture, context reporting, and the optional
  `elf3d_render_benchmark` executable.

## Measured Results

On a 42,173-primitive, 11.85-million-triangle position/normal workload, the
instrumented 1600x900 steady-frame median fell from 764.33 ms to 102.61 ms and
p95 fell from 801.82 ms to 104.05 ms. Compact vertex layouts reduced estimated
resident geometry from 982,721,928 to 502,429,512 bytes, a 458.0 MiB saving.

The workload remains draw- and vertex-heavy. Standard PBR, RGBA16F rendering,
VSync, focus-depth anchoring, and 100% render scale remain enabled by default;
unlit rendering and lower diagnostic scales are explicit comparison controls.

## Public Interface Additions

- `RenderStatistics` now reports candidate, visible, and culled primitives;
  passes and state switches; uploads and resident bytes; CPU phases; and
  delayed main/resolve GPU time.
- `PickingStatistics` now reports pass, readback, allocation, CPU, and delayed
  GPU timing.
- `RenderShadingMode`, viewport shading accessors, and
  `Viewport::render_revision()` support diagnostic rendering and host-side
  retained-frame decisions.
- `OrbitNavigationSettings::focus_depth_anchor_enabled` permits an explicit
  navigation comparison without changing its default behavior.

These additions extend public structs and interfaces. Rebuild C++ clients
against the 0.8.7 headers and library, consistent with Elf3D's documented
same-toolchain C++ compatibility contract.

## Requirements

- Windows x64.
- OpenGL 4.1 core-profile graphics driver.
- Microsoft Visual C++ Redistributable for Visual Studio 2022.

## License

Elf3D source is available under the MIT License. Third-party software and
visual subjects retain the licenses and notices included in the source tree
and viewer package.
