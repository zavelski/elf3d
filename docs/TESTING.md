# Testing Elf3D

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
permanent iterative Document-to-Scene adapter with a 1,024-level hierarchy.

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
