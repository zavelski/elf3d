# Elf3D Ordinary Publication Validation Policy

Use this matrix to classify an ordinary change and select validation. Record
the exact commands run and actual test totals. Do not hard-code historical test
counts.

## Classification

Classify the work as one or more of:

- documentation-only;
- C++ implementation;
- public API;
- tests;
- shaders;
- rendering;
- Scene or Assets;
- Viewport or Tools;
- CMake;
- GitHub Actions;
- dependencies;
- packaging infrastructure;
- performance work.

## Documentation Maintenance

Review affected living documents:

- `README.md`
- `CHANGELOG.md`
- `docs/PUBLIC_API_OVERVIEW.md`
- `docs/MODULE_MAP.md`
- `docs/GLTF_SUPPORT.md`
- `docs/RENDERING_PIPELINE.md`
- `docs/VIEWPORT_AND_TOOLS.md`
- `docs/LIFETIME_AND_THREADING.md`
- `docs/TESTING.md`
- `docs/PERFORMANCE_BASELINE.md`
- `docs/ROADMAP.md`
- `docs/USER_GUIDE.md`

Do not rewrite historical release snapshots for ordinary development changes.
Do not describe planned functionality as implemented.

## Validation Matrix

Documentation-only:

- run repository consistency checks;
- run `git diff --check`;
- run Markdown referenced-path checks where supported;
- run YAML checks where supported;
- full binary builds are unnecessary unless build commands, configuration,
  public examples, package metadata, CI, or release instructions changed.

C++ source, headers, shaders, Scene, Renderer, Viewport, or Tools:

- `cmake --fresh --preset windows-debug`
- `cmake --build --preset windows-debug --parallel`
- `ctest --preset windows-debug --output-on-failure`
- `cmake --fresh --preset windows-release`
- `cmake --build --preset windows-release --parallel`
- `ctest --preset windows-release --output-on-failure`

Public API or ABI:

- run the C++ matrix;
- inspect exported declarations;
- verify public-header self-containment where possible;
- update API documentation;
- identify compatibility consequences;
- review DLL allocation, destruction, and ownership boundaries.

CMake, presets, dependencies, or GitHub workflows:

- run the C++ matrix unless the change is provably documentation-only;
- validate configuration syntax;
- inspect `CMakePresets.json`;
- validate workflow YAML;
- verify remote CI after push.

Viewer interaction:

- run the C++ matrix;
- launch the viewer where possible;
- perform applicable smoke validation;
- state which interactive checks remain manual.

Packaging work:

- run the C++ matrix;
- generate the Release package;
- inspect archive contents;
- extract or otherwise verify the package in a clean location;
- launch the packaged viewer when possible;
- generate SHA-256 checksums.

Performance work:

- run the C++ matrix;
- run or add representative benchmarks;
- record baseline, changed result, environment, and measurement limits.
