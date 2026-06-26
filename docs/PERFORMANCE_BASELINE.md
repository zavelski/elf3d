# Performance Baseline

Purpose: Record measured and unmeasured performance status for Elf3D 0.3.0 and
provide reproducible measurement procedures.

Applicable version: 0.3.0

Document status: Baseline procedure only. No performance benchmark run has been
performed.

Last verified Git commit: pending C++20 module migration commit

Implementation source paths: `apps/viewer`, `modules/gltf`, `modules/renderer`,
`modules/picking`, `modules/scene`, `tests/fixtures/textured_pbr.gltf`

Known limitations: No timing instrumentation, benchmark target, or memory
measurement workflow is committed. Do not treat CTest wall times as product
performance numbers.

Related documents: `TESTING.md`, `RENDERING_PIPELINE.md`, `GLTF_SUPPORT.md`,
`ROADMAP.md`

## Measurement Status

| Metric | Status | Reason |
| --- | --- | --- |
| Hardware | Not measured | No benchmark machine profile recorded. |
| Operating system | Not measured | Validation environment was not captured as a benchmark baseline. |
| Compiler | Known for validation | MSVC 19.44.35228.0 used for build validation, not benchmarking. |
| Build configuration | Known for validation | Debug and Release built; no timed benchmark run. |
| Graphics driver | Not measured | No driver version captured. |
| Test model | Available | `tests/fixtures/textured_pbr.gltf` is available for small visual validation. |
| File size | Not measured | Procedure below should record it. |
| Entity count | Not measured | Can be read from viewer Model Information. |
| Mesh count | Not measured | Can be read from viewer Model Information. |
| Primitive count | Not measured | Can be read from viewer Model Information. |
| Triangle count | Not measured | Can be read from viewer Model Information. |
| Image and texture count | Not measured | Can be read from viewer Model Information. |
| Scene-load time | Not measured | No load timer exposed in public docs. |
| Decode time | Not measured | No decode timer exposed. |
| GPU-upload time | Not measured | Renderer reports upload counts, not time. |
| First-frame latency | Not measured | Requires manual timing or instrumentation. |
| Steady-state frame time | Not measured | Viewer displays FPS but no benchmark capture is recorded. |
| Draw calls | Not measured | Available through `RenderStatistics`. |
| GPU-cache hit rates | Not measured | Counts exist but no benchmark capture is recorded. |
| BVH-build time | Not measured | Picking statistics count builds, not time. |
| Picking-query time | Not measured | Requires instrumentation. |
| Hierarchy-snapshot time | Not measured | Requires instrumentation. |
| Clipping overhead | Not measured | Requires comparative benchmark. |
| Memory usage | Not measured | Requires OS/process measurement. |

## Minimum Reproducible Procedure

1. Record machine details:
   - CPU
   - GPU
   - RAM
   - Windows version
   - graphics driver version
   - Visual Studio/MSVC version
2. Configure and build Release:

```powershell
cmake --fresh --preset windows-release
cmake --build --preset windows-release
```

3. Launch the viewer with a fixed model:

```powershell
.\out\build\windows-release\bin\Release\elf3d_viewer.exe tests\fixtures\textured_pbr.gltf
```

4. Record from the viewer:
   - entity, mesh, material, image, texture, primitive, vertex, index, triangle counts
   - decoded image bytes
   - draw calls
   - rendered triangles
   - texture bindings/uploads
   - current GPU textures
   - picking cache counters after a fixed pick sequence
5. Use an external timer or added instrumentation to record:
   - load time
   - first-frame time
   - steady-state frame time over a fixed camera view
   - picking query time
   - memory usage
6. Commit benchmark scripts or raw data only when the procedure is repeatable
   and the model data is project-owned or properly licensed.

## Current Baseline Decision

No performance claims should be made for 0.3.0 beyond the existence of renderer,
picking, and cache statistics. The first real performance baseline should be a
future task after release documentation and manual validation are complete.
