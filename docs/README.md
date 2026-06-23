# Elf3D Documentation Index

Purpose: Index the verified Elf3D technical documentation set.

Applicable version: 0.1.0

Document status: Living index, created from the 0.1.0 audit branch.

Last verified Git commit: `8504068`

Implementation source paths: `include/elf3d`, `modules`, `facade/elf3d`,
`integrations/imgui`, `apps/viewer`, `tests`, `CMakeLists.txt`,
`CMakePresets.json`

Known limitations: This index is not a release snapshot. Release-specific
copies belong under `docs/releases/<version>/`.

Related documents: `../PROJECT_STATE_EN.md`, `../README.md`,
`audits/ELF3D_0.1.0_AUDIT.md`

## Core Documents

- `PUBLIC_API_OVERVIEW.md`: public C++ API, ownership, errors, integration.
- `MODULE_MAP.md`: CMake targets, module responsibilities, dependencies.
- `GLTF_SUPPORT.md`: verified glTF and GLB support matrix.
- `RENDERING_PIPELINE.md`: graphics abstraction, OpenGL backend, renderer.
- `VIEWPORT_AND_TOOLS.md`: viewport input, navigation, picking, tools.
- `LIFETIME_AND_THREADING.md`: ownership, shutdown order, thread affinity.
- `TESTING.md`: build, test, fixture, and manual validation procedure.
- `PERFORMANCE_BASELINE.md`: measurement status and benchmark procedure.
- `ROADMAP.md`: release blockers, 0.1.x corrections, future candidates.
- `USER_GUIDE.md`: actual viewer usage and limitations.

## Audit Documents

- `audits/ELF3D_0.1.0_REPOSITORY_INVENTORY.md`
- `audits/ELF3D_0.1.0_AUDIT.md`
- `audits/ELF3D_0.1.0_VALIDATION_MATRIX.md`
- `audits/ELF3D_0.1.0_REMEDIATION_PLAN.md`
- `audits/ELF3D_0.1.0_REMEDIATION_LOG.md`

## Current Release Readiness

0.1.0 is not tagged yet. Remaining gates are release snapshot preparation,
manual viewer validation, release checklist completion, and the final release
decision in Goals 6 and 7.
