# Elf3D 0.1.0 Release Checklist

Purpose: Record the release-readiness decision for publishing Elf3D 0.1.0.

Applicable version: 0.1.0

Document status: Publication release checklist.

Last verified implementation commit: `eeb39cdb2a9e92e61001a00d11cbe1880716f921`

Implementation source paths: `CMakeLists.txt`, `CMakePresets.json`,
`.github/workflows`, `scripts/package_release.ps1`, `include/elf3d`,
`modules`, `facade/elf3d`, `apps/viewer`, `tests`, `LICENSE`,
`THIRD_PARTY.md`, `docs`

Known limitations: Public publication is blocked until the full manual viewer
interaction matrix is completed and recorded.

Related documents: `PROJECT_STATE_EN.md`, `AUDIT_SUMMARY.md`,
`VALIDATION_SUMMARY.md`, `KNOWN_LIMITATIONS.md`, `PUBLICATION_PRECHECK.md`,
`PUBLIC_CONTENT_AUDIT.md`, `RELEASE_ARTIFACTS.md`

## Decision

`NO-GO — publication blocked`

Do not create `v0.1.0`, push branches or tags, create the GitHub Release, or
upload release assets while the blocker below remains.

## Checklist

| Gate | Status | Evidence |
| --- | --- | --- |
| Public content audit | Passed | `PUBLIC_CONTENT_AUDIT.md` found no content blockers. |
| Project license | Passed | Root `LICENSE` is standard MIT with `Copyright (c) 2026 Serge Zavelski`; README identifies MIT. |
| Third-party licenses | Passed | `THIRD_PARTY.md` and `third_party/licenses/` preserve dependency notices separately. |
| Clean tracked-file hygiene | Passed | Build outputs and local runtime files are ignored; no tracked large/generated binaries found. |
| Version consistency | Passed | CMake, runtime version data, public API test, docs, package filenames, and release notes use `0.1.0`. |
| Debug configure | Passed | Fresh `windows-debug` preset configured. |
| Debug build | Passed | `cmake --build --preset windows-debug`. |
| Debug tests | Passed | `ctest --preset windows-debug --output-on-failure`; 16/16 passed. |
| Release configure | Passed with local workaround | Fresh `windows-release` preset configured after temporary per-process Git safe-directory entries for generated FetchContent checkouts on `Z:`. |
| Release build | Passed | `cmake --build --preset windows-release`. |
| Release tests | Passed | `ctest --preset windows-release --output-on-failure`; 16/16 passed. |
| Viewer smoke test | Passed with limitation | Debug and Release viewers opened the project-owned fixture and exited with code 0 after `CloseMainWindow()`. |
| Visual fixture rendering | Passed with limitation | Screenshot showed the fixture rendered in the Release viewer. |
| Manual viewer interaction matrix | Not tested | Navigation, picking, selection, visibility, isolation, measurement, and clipping were not manually exercised end to end. |
| OpenGL shutdown | Partially verified | Window close path returned exit code 0 for Debug, Release, and packaged Release viewer. |
| Release archive inspection | Passed | ZIP file contents matched `RELEASE_ARTIFACTS.md`. |
| Checksums | Passed | `SHA256SUMS.txt` generated and verified against the ZIP. |
| SDK package | Not applicable | Deferred for 0.1.0; no SDK archive is produced. |
| Documentation | Passed with limitation | Public docs, release notes, CI docs, and package docs were updated; older audit docs remain historical. |
| Remote repository safety | Not tested | GitHub repository inspection not run because local decision is no-go. |
| Tag correctness | Not tested | `v0.1.0` is not created while no-go remains. |
| GitHub CI | Not tested | Workflows are committed locally but not run remotely. |
| GitHub Release | Not tested | Release not created. |
| Clone test | Not tested | Public repository not published. |

## Remaining Blocker

- Full manual viewer interaction validation has not been performed. Required
  coverage: failed-load preservation, orbit, pan, wheel dolly, fit, reset,
  picking, selection, hierarchy visibility, isolation, distance measurement,
  section plane, clipping boxes, reload, and close scene.

## Publication Status

Publication is intentionally stopped before Goal 7. The local preparation
commits are preserved. Resume publication only after the remaining manual
viewer interaction validation is completed and this checklist records:

`GO — ready for public publication`
