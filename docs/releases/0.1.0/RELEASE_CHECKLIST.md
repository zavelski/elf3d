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

Known limitations: No local publication blocker remains after the
user-performed packaged viewer interaction validation. Remote repository and
branch CI validation passed; tag-triggered release, GitHub Release, and
clone-test validation remain pending publication steps.

Related documents: `PROJECT_STATE_EN.md`, `AUDIT_SUMMARY.md`,
`VALIDATION_SUMMARY.md`, `KNOWN_LIMITATIONS.md`, `PUBLICATION_PRECHECK.md`,
`PUBLIC_CONTENT_AUDIT.md`, `RELEASE_ARTIFACTS.md`

## Decision

`GO — ready for public publication`

Proceed with tag publication, tag-triggered release workflow verification,
GitHub Release verification, and public clone validation. If any required
validation fails, restore the release decision to no-go before continuing
publication.

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
| Tag correctness | Pending publication step | Local annotated `v0.1.0` has been recreated for the corrected release commit; remote `v0.1.0` has not been pushed yet. |
| GitHub CI | Passed | Corrected `develop` and `main` CI passed Debug and Release jobs on `windows-2022`. |
| GitHub Release | Pending publication step | Release has not been created yet. |
| Clone test | Pending publication step | Fresh public clone from the published `v0.1.0` tag has not been run yet. |

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

Local publication readiness is confirmed. Continue the publication sequence
only while local validation, remote branch CI, tag-triggered release workflow,
GitHub Release creation, and public clone validation continue to pass.

`GO — ready for public publication`
