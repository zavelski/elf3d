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

The suite covers scene and asset behavior, math, importing, image decoding,
navigation, picking, viewer tools, rendering preparation, viewport lifetime,
the public API, and OpenGL rendering.

After building, a focused group can be run with a CTest expression:

```powershell
ctest --preset windows-debug -R "elf3d\.(scene|picking)" --output-on-failure
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
