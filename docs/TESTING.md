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

The suite covers scene and asset behavior, math, importing, image decoding,
navigation, picking, viewer tools, rendering preparation, viewport lifetime,
the public API, model document and asset-reference behavior, and OpenGL
rendering. The model-only suite stops before renderer, backend OpenGL,
viewport, ImGui, GLFW, and viewer targets. It covers Document construction and
processing, all-scene glTF import, glTF/GLB export, source-image and raw-metadata
fidelity, and verifies from generated CMake metadata that Scene/Assets and
engine/UI targets were not configured.

The named `elf3d.scene_runtime_adapter_depth` regression exercises the
permanent iterative Document-to-Scene adapter with a 5,120-level hierarchy.

CI runs the full Debug suite and an independent model-only Debug suite for
every push and pull request. A weekly schedule and manual dispatch additionally
run both Release suites. All jobs use the project's current pinned CMake 4.3.4
baseline rather than testing older CMake compatibility. The standard hosted
Windows environment does not guarantee an OpenGL 4.1 runtime, so the two
hidden-context graphics tests may report `Skipped`; that result is not evidence
of real rendering. A hard graphics gate requires a runner that guarantees a
compatible context.

After building, a focused group can be run with a CTest expression:

```powershell
ctest --preset windows-debug -R "elf3d\.(scene|picking)" --output-on-failure
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
