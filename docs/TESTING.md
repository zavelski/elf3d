# Testing Elf3D

## Preset Contracts

After changing CMake options, presets, or product composition, validate all
four checked-in configure and target contracts:

```powershell
.\cmake\check-preset-contracts.ps1
```

The check configures isolated full and model-only Debug/Release trees below
`out/`, verifies required and forbidden targets, and removes those trees after
completion.

## Automated Tests

Configure, build, and run the Debug suite:

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug --parallel
ctest --preset windows-debug --output-on-failure
```

Run the Release suite:

```powershell
cmake --preset windows-release
cmake --build --preset windows-release --parallel
ctest --preset windows-release --output-on-failure
```

Run the model-only suite:

```powershell
cmake --preset windows-model-debug
cmake --build --preset windows-model-debug --parallel
ctest --preset windows-model-debug --output-on-failure
```

The suite covers the standard application lifecycle and failure paths,
normalized input and interaction arbitration, scene assets and surface anchors,
navigation, picking, independent viewport selection/visibility/clipping state,
generic overlays, viewer-owned Tools and Workflows, typed command
FIFO/replacement barriers, separate Component-state ownership,
preferences/browser behavior, viewer shell smoke behavior, rendering
preparation, viewport lifetime, the public API, model document and
asset-reference behavior, and OpenGL rendering. A separate external target
copies and adapts a Measurement Tool while seeing only public Elf3D headers and
targets; it does not link the viewer or use private implementation includes.

The context-dependent `elf3d.render_quality_material_pixels` test renders a
generated white/dielectric/polished-metal/rough-metal scene and enforces the
neutral studio's luminance, clipping, and highlight criteria. The optional
`elf3d_render_quality_capture` executable can write a resolved PNG plus
path-free JSON camera/settings metadata for local comparisons.

The model-only suite stops before renderer, backend OpenGL, viewport, Standard
Application Framework, embedding integration, ImGui, GLFW, and viewer targets.
It covers Document construction and processing, all-scene glTF import,
glTF/GLB export, source-image and raw-metadata fidelity, and verifies from
generated CMake metadata that Scene/Assets and engine/UI targets were not
configured. Preset-local test scratch directories keep concurrent full and
model-only profiles independent.

The named `elf3d.scene_runtime_adapter_depth` regression exercises the
permanent iterative Document-to-Scene adapter with a 5,120-level hierarchy.
The `elf3d.measurement_copy_adapt` regression exercises the external
public-only Measurement Tool through a hidden Standard Application driver.
The viewer smoke verifies that the copied `DroidSans.ttf` asset is the default
presentation font, then hides and restores the docked 3D View before completing
its render sequence, guarding interaction-region recreation as well as launch
and teardown.

CI runs the full Debug suite and an independent model-only Debug suite for
every push and pull request. A weekly schedule and manual dispatch additionally
run both Release suites. All jobs use the project's current pinned CMake 4.3.4
baseline rather than testing older CMake compatibility. The standard hosted
Windows environment does not guarantee an OpenGL 4.1 runtime, so the
context-dependent application, viewer, rendering, and integration smoke tests
may report `Skipped`; that result is not evidence
of real rendering. A hard graphics gate requires a runner that guarantees a
compatible context.

After building, a focused group can be run with a CTest expression:

```powershell
ctest --preset windows-debug -R "elf3d\.(scene|picking)" --output-on-failure
ctest --preset windows-debug -R "elf3d\.(application_smoke|interaction_arbiter|viewer_(behavior|smoke))" --output-on-failure
ctest --preset windows-debug -R "elf3d\.measurement_copy_adapt" --output-on-failure
ctest --preset windows-debug -R "elf3d\.scene_runtime_adapter_depth" --output-on-failure
ctest --preset windows-model-debug -R "elf3d\.model_" --output-on-failure
```

## Viewer Check

Launch the checked-in smoke model:

```powershell
.\out\build\windows-debug\bin\Debug\elf3d_viewer.exe `
    .\tests\fixtures\elf3d_smoke\elf3d_smoke.gltf
```

Check model loading, orbit/pan/dolly navigation, selection, visibility,
measurement, clipping, resize, and clean shutdown.

The smoke model and its license are stored in
`tests/fixtures/elf3d_smoke/`.
