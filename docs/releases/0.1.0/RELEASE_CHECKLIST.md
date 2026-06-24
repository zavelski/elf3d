# Elf3D 0.1.0 Release Checklist

Purpose: Record the release-readiness decision for publishing Elf3D 0.1.0.

Applicable version: 0.1.0

Document status: Publication release checklist.

Last verified implementation commit before final package-record update:
`a99bb1008882994d3127141019b049927cbc2c97`

Implementation source paths: `CMakeLists.txt`, `CMakePresets.json`,
`.github/workflows`, `scripts/package_release.ps1`, `include/elf3d`,
`modules`, `facade/elf3d`, `apps/viewer`, `tests`, `LICENSE`,
`THIRD_PARTY.md`, `docs`

Known limitations: Public publication validation passed. Remaining non-blocking
limitations are recorded in `KNOWN_LIMITATIONS.md` and `PUBLICATION_REPORT.md`.

Related documents: `PROJECT_STATE_EN.md`, `AUDIT_SUMMARY.md`,
`VALIDATION_SUMMARY.md`, `KNOWN_LIMITATIONS.md`, `PUBLICATION_PRECHECK.md`,
`PUBLIC_CONTENT_AUDIT.md`, `RELEASE_ARTIFACTS.md`

## Decision

`GO — ready for public publication`

Public publication completed on 2026-06-24. Do not move or recreate
`v0.1.0` merely to add post-publication documentation.

## Checklist

| Gate | Status | Evidence |
| --- | --- | --- |
| Public content audit | Passed | `PUBLIC_CONTENT_AUDIT.md` found no content blockers. |
| Project license | Passed | Root `LICENSE` is standard MIT with `Copyright (c) 2026 Serge Zavelski`; README identifies MIT. |
| Third-party licenses | Passed | `THIRD_PARTY.md` and `third_party/licenses/` preserve dependency notices separately. |
| Clean tracked-file hygiene | Passed | Build outputs and local runtime files are ignored; no tracked large/generated binaries found. |
| Version consistency | Passed | CMake, runtime version data, public API test, docs, package filenames, and release notes use `0.1.0`. |
| Debug configure | Passed | Fresh `windows-debug` preset configured after the `GO` decision. |
| Debug build | Passed | `cmake --build --preset windows-debug`. |
| Debug tests | Passed | `ctest --preset windows-debug --output-on-failure`; 16/16 passed. |
| Release configure | Passed | Fresh `windows-release` preset configured after the `GO` decision. |
| Release build | Passed | `cmake --build --preset windows-release`. |
| Release tests | Passed | `ctest --preset windows-release --output-on-failure`; 16/16 passed. |
| Viewer smoke test | Passed with limitation | Debug and Release viewers opened the project-owned fixture and exited with code 0 after `CloseMainWindow()`. |
| Visual fixture rendering | Passed with limitation | Screenshot showed the fixture rendered in the Release viewer. |
| Manual viewer interaction matrix | Passed | User-performed validation on the packaged Windows Release viewer covered navigation, picking, selection, hierarchy synchronization, visibility, isolation, measurement, clipping, reload, close-scene, failed-load preservation, and normal shutdown. |
| OpenGL shutdown | Partially verified | Window close path returned exit code 0 for Debug, Release, and packaged Release viewer. |
| Release archive inspection | Passed | ZIP file contents matched `RELEASE_ARTIFACTS.md`. |
| Checksums | Passed | `SHA256SUMS.txt` generated and verified against the ZIP: `1d39c50460e86083f448557ed6a7eddad3974d26b99e84e4c2cfc030c5265c92`. |
| SDK package | Not applicable | Deferred for 0.1.0; no SDK archive is produced. |
| Documentation | Passed with limitation | Public docs, release notes, CI docs, and package docs were updated; older audit docs remain historical. |
| Remote repository safety | Passed | GitHub repository `zavelski/elf3d` is public, `origin` points to it, and remote `main`/`develop` were inspected. |
| Tag correctness | Passed | Remote annotated `v0.1.0` tag object `24c3357ab0ae4aa20bc6be8d3de6403e30158e00` peels to `53047abef3f7e7c31d82913c1e9642d5f1b0d294`. |
| GitHub CI | Passed | Corrected `develop` and `main` CI passed Debug and Release jobs on `windows-2022`. |
| GitHub Release | Passed | Public release `Elf3D 0.1.0` exists at `https://github.com/zavelski/elf3d/releases/tag/v0.1.0`; it is not draft or prerelease, and it is marked latest. |
| Clone test | Passed | Fresh public clone checked out `v0.1.0`, configured, built, and passed Debug and Release CTest 16/16. |

## Manual Viewer Interaction Validation

User-performed validation on the packaged Windows Release viewer passed:
orbit navigation, pan navigation, wheel zoom, Fit to Scene, Reset View,
Viewport picking, object selection and highlighting, Viewport selection to
Scene Hierarchy synchronization, Hide Selected, Show Selected, Show All,
inherited hierarchy visibility, Isolate Selected, Exit Isolation,
point-to-point distance measurement, measurement stability during camera
navigation, section-plane clipping, retained-side switching, clipping boxes,
Clear Clipping, rejection of clipped geometry by picking, Reload, Close Scene,
loading a Scene after Close Scene, failed-load preservation of the previously
active Scene, and normal viewer shutdown.

This was manual GUI validation performed by the user, not an automated test.

## Remaining Local Blockers

None recorded.

## Publication Status

Public publication is complete. Post-publication documentation may be committed
to `develop`, but the existing `v0.1.0` tag must remain unchanged.

`GO — ready for public publication`
