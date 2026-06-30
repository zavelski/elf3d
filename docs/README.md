# Elf3D Documentation Index

Purpose: Index the verified Elf3D technical documentation set.

Applicable version: 0.7.1

Document status: Living index for the local 0.7.1 source.

Last verified Git commit: local tag `v0.7.1` after release commit

Implementation source paths: `include/elf3d`, `modules`, `facade/elf3d`,
`integrations/imgui`, `apps/viewer`, `tests`, `CMakeLists.txt`,
`CMakePresets.json`

Known limitations: This index is not an immutable release snapshot. The latest
public release records remain under `releases/0.2.0/`; the current local
release records are under `releases/0.7.1/`.

Related documents: `../PROJECT_STATE_EN.md`, `../README.md`,
`releases/0.7.1/RELEASE_CHECKLIST.md`

## Core Documents

- `PUBLIC_API_OVERVIEW.md`: public C++ API, ownership, errors, integration.
- `MODULE_MAP.md`: CMake targets, OBJECT-library groups, named modules, dependencies.
- `GLTF_SUPPORT.md`: verified glTF and GLB support matrix.
- `RENDERING_PIPELINE.md`: graphics abstraction, OpenGL backend, renderer.
- `VIEWPORT_AND_TOOLS.md`: viewport input, navigation, picking, tools.
- `LIFETIME_AND_THREADING.md`: ownership, shutdown order, thread affinity.
- `TESTING.md`: build, test, fixture, and manual validation procedure.
- `PERFORMANCE_BASELINE.md`: measurement status and benchmark procedure.
- `ROADMAP.md`: release blockers, 0.1.x corrections, future candidates.
- `USER_GUIDE.md`: actual viewer usage and limitations.
- `CODEX_SKILLS.md`: repository-local automation skill routing.
- `DOCUMENTATION_POLICY.md`: documentation ownership and review rules.
- `DOCUMENTATION_UPDATE_CHECKLIST.md`: task checklist for documentation updates.

## 0.7.1 Local Release Records

- `releases/0.7.1/PROJECT_STATE_EN.md`
- `releases/0.7.1/VALIDATION_SUMMARY.md`
- `releases/0.7.1/KNOWN_LIMITATIONS.md`
- `releases/0.7.1/RELEASE_CHECKLIST.md`
- `releases/0.7.1/RELEASE_ARTIFACTS.md`
- `releases/0.7.1/PUBLIC_CONTENT_AUDIT.md`
- `releases/0.7.1/GITHUB_RELEASE_NOTES.md`

## Previous 0.6.0 Local Release Records

- `releases/0.6.0/PROJECT_STATE_EN.md`
- `releases/0.6.0/VALIDATION_SUMMARY.md`
- `releases/0.6.0/KNOWN_LIMITATIONS.md`
- `releases/0.6.0/RELEASE_CHECKLIST.md`
- `releases/0.6.0/RELEASE_ARTIFACTS.md`
- `releases/0.6.0/PUBLIC_CONTENT_AUDIT.md`
- `releases/0.6.0/GITHUB_RELEASE_NOTES.md`

## Previous 0.4.0 Local Release Candidate

- `releases/0.4.0/PROJECT_STATE_EN.md`
- `releases/0.4.0/VALIDATION_SUMMARY.md`
- `releases/0.4.0/KNOWN_LIMITATIONS.md`
- `releases/0.4.0/RELEASE_CHECKLIST.md`
- `releases/0.4.0/RELEASE_ARTIFACTS.md`
- `releases/0.4.0/PUBLIC_CONTENT_AUDIT.md`
- `releases/0.4.0/GITHUB_RELEASE_NOTES.md`

## Latest Public Release Snapshot

- `releases/0.2.0/PROJECT_STATE_EN.md`
- `releases/0.2.0/VALIDATION_SUMMARY.md`
- `releases/0.2.0/KNOWN_LIMITATIONS.md`
- `releases/0.2.0/RELEASE_CHECKLIST.md`
- `releases/0.2.0/RELEASE_ARTIFACTS.md`
- `releases/0.2.0/GITHUB_RELEASE_NOTES.md`

## Previous Release Snapshot

- `releases/0.1.0/PROJECT_STATE_EN.md`
- `releases/0.1.0/AUDIT_SUMMARY.md`
- `releases/0.1.0/VALIDATION_SUMMARY.md`
- `releases/0.1.0/KNOWN_LIMITATIONS.md`
- `releases/0.1.0/RELEASE_CHECKLIST.md`
- `releases/0.1.0/PUBLICATION_PRECHECK.md`
- `releases/0.1.0/PUBLIC_CONTENT_AUDIT.md`

## Audit Documents

- `audits/ELF3D_0.1.0_REPOSITORY_INVENTORY.md`
- `audits/ELF3D_0.1.0_AUDIT.md`
- `audits/ELF3D_0.1.0_VALIDATION_MATRIX.md`
- `audits/ELF3D_0.1.0_REMEDIATION_PLAN.md`
- `audits/ELF3D_0.1.0_REMEDIATION_LOG.md`

## Current Release Readiness

The 0.7.1 source, local package, and automated validation are tracked under
`releases/0.7.1/`. GitHub publication, CI, release asset upload, and public
clone verification are intentionally deferred to manual follow-up. The latest
immutable public release snapshot remains 0.2.0.
