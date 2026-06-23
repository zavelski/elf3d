# Contributing To Elf3D

Thank you for considering a contribution to Elf3D.

## Scope

Elf3D is a modular C++20 3D visualization engine. Keep changes focused on the
current architecture: public `elf3d` API, internal engine modules, optional
ImGui integration, reference viewer, tests, documentation, and release tooling.

Do not add speculative systems such as an ECS, universal plugin framework,
service locator, custom memory manager, or additional graphics backend unless a
current task requires it.

## Before Opening An Issue

For bugs, include:

- Elf3D version or commit.
- Windows version.
- Visual Studio and MSVC version.
- CMake version and preset.
- Graphics hardware and driver version.
- Reproduction steps.
- Expected and actual behavior.
- Whether the model is `.gltf` or `.glb`, approximate size, image formats, and
  whether it uses animation, skins, morphs, compression, transparency, or KTX2.

Do not upload confidential customer models, proprietary building data, CET or
Revit exports, or license-ambiguous assets. Reduce the issue to a synthetic or
redistributable fixture when possible.

## Pull Requests

- Read `AGENTS.md`, `ARCHITECTURE.md`, and `CODING_POLICY.md`.
- Use the existing CMake presets.
- Keep public API changes deliberate and documented.
- Keep third-party types out of public Elf3D headers.
- Add focused tests for behavior changes.
- Update documentation when behavior, validation, or release status changes.

Expected local validation for most changes:

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug --output-on-failure
```

Run Release validation when touching CMake, optimization-sensitive code,
packaging, public API, or release files.

## License

By contributing, you agree that your original contribution may be distributed
under the MIT License used by Elf3D. Third-party code or assets must be clearly
identified, compatible with redistribution, and documented in `THIRD_PARTY.md`
when included.
