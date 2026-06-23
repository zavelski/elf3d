# Elf3D 0.1.0 Release Checklist

Purpose: Record the release-readiness decision for the Elf3D 0.1.0 release
candidate.

Applicable version: 0.1.0

Document status: Release snapshot checklist.

Last verified implementation commit before release snapshot: `79fd4bc`

Implementation source paths: `CMakeLists.txt`, `CMakePresets.json`,
`include/elf3d`, `modules`, `facade/elf3d`, `apps/viewer`, `tests`,
`THIRD_PARTY.md`, `docs`

Known limitations: The checklist blocks release because manual visual viewer
validation has not been performed.

Related documents: `PROJECT_STATE_EN.md`, `AUDIT_SUMMARY.md`,
`VALIDATION_SUMMARY.md`, `KNOWN_LIMITATIONS.md`

## Decision

`Not ready due to release blockers`

Do not execute Goal 8. Do not merge to `develop`, create `main`, or create the
annotated `v0.1.0` tag while the blocker below remains.

## Checklist

| Gate | Status | Evidence |
| --- | --- | --- |
| Clean Git state before release docs | Passed | `git status --short --ignored` showed only ignored `imgui.ini` and `out/`. |
| Version consistency | Passed | CMake project version, runtime version data, public API test, viewer About path, and docs refer to `0.1.0`. |
| Debug configure | Passed | `cmake --fresh --preset windows-debug`. |
| Debug build | Passed | `cmake --build --preset windows-debug`; no warning diagnostics observed. |
| Debug tests | Passed | `ctest --preset windows-debug --output-on-failure`; 16/16 passed. |
| Release configure | Passed | `cmake --fresh --preset windows-release`. |
| Release build | Passed | `cmake --build --preset windows-release`; no warning diagnostics observed. |
| Release tests | Passed | `ctest --preset windows-release --output-on-failure`; 16/16 passed. |
| Public-header self-containment | Passed | All public headers under `include/elf3d` compiled individually as forced includes with MSVC C++20, `/permissive-`, `/W4`, `/WX`. |
| Viewer smoke test | Passed with limitation | Debug and Release viewers stayed alive for five seconds with `tests/fixtures/textured_pbr.gltf`, then were intentionally terminated. |
| glTF fixture loading | Partially verified | Viewer process accepted the fixture path and remained alive; visual rendering was not inspected. |
| OpenGL shutdown | Not verified | Smoke test terminated the process; normal user-driven shutdown was not validated. |
| Documentation completeness | Passed | Required path check covered 43 paths; Markdown link check covered 31 Markdown files. |
| Known limitations | Passed | Documented in `KNOWN_LIMITATIONS.md` and `CHANGELOG.md`. |
| Audit blockers | Blocked | Manual visual viewer validation remains incomplete. |
| License files | Passed by audit | `THIRD_PARTY.md` records pinned dependencies and license notice files. |
| Tag readiness | Failed | Release decision is `Not ready due to release blockers`. |

## Remaining Blocker

- Manual visual viewer validation has not been performed. Required manual
  coverage: startup and shutdown, procedural cube rendering, fixture rendering,
  failed-load preservation, orbit, pan, wheel dolly, fit, reset, picking,
  selection, hierarchy visibility, isolation, distance measurement, section
  plane, clipping boxes, reload, close scene, and OpenGL shutdown behavior.

## Goal 8 Status

Goal 8 is intentionally not executed.
