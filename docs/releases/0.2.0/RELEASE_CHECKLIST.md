# Elf3D 0.2.0 Release Checklist

Purpose: Record the release-readiness decision for publishing Elf3D 0.2.0.

Applicable version: 0.2.0

Document status: Release checklist.

Last verified implementation commit: pending 0.2.0 release source commit

Implementation source paths: `CMakeLists.txt`, `CMakePresets.json`,
`.github/workflows`, `scripts/package_release.ps1`, `include/elf3d`,
`modules`, `facade/elf3d`, `apps/viewer`, `tests`, `LICENSE`,
`THIRD_PARTY.md`, `docs`

Known limitations: Remaining non-blocking limitations are recorded in
`KNOWN_LIMITATIONS.md`.

Related documents: `PROJECT_STATE_EN.md`, `VALIDATION_SUMMARY.md`,
`KNOWN_LIMITATIONS.md`, `PUBLIC_CONTENT_AUDIT.md`, `RELEASE_ARTIFACTS.md`

## Decision

`GO - ready for public release`

## Checklist

| Gate | Status | Evidence |
| --- | --- | --- |
| Public content audit | Passed | `PUBLIC_CONTENT_AUDIT.md` found no content blockers. |
| Project license | Passed | Root `LICENSE` is MIT. |
| Third-party licenses | Passed | `THIRD_PARTY.md` and `third_party/licenses/` include dependency notices, including Droid Sans Apache-2.0. |
| Version consistency | Passed | CMake, runtime version data, public API test, docs, package filenames, and release notes use `0.2.0`. |
| Debug configure | Passed | `cmake --fresh --preset windows-debug`. |
| Debug build | Passed | `cmake --build --preset windows-debug --parallel`. |
| Debug tests | Passed | `ctest --preset windows-debug --output-on-failure`; 16/16 passed. |
| Release configure | Passed | `cmake --fresh --preset windows-release`. |
| Release build | Passed | `cmake --build --preset windows-release --parallel`. |
| Release tests | Passed | `ctest --preset windows-release --output-on-failure`; 16/16 passed. |
| Viewer smoke test | Passed | Release viewer opened with the project fixture and exited with code 0 after `CloseMainWindow()`. |
| Visual comparison | Passed with limitation | Low.3D-style UI was compared during ordinary-change validation; additional release-clone Computer Use capture was blocked by Windows access denial. |
| Release archive inspection | Passed | ZIP contents matched `RELEASE_ARTIFACTS.md`. |
| Checksums | Passed | `SHA256SUMS.txt` matched ZIP SHA-256 `8cdd519ccee832fb8705a307eba32e46759e8920fef37ea2993a0b85b599c3e4`. |
| SDK package | Not applicable | Deferred for 0.2.0; no SDK archive is produced. |
| Documentation | Passed | Living docs and 0.2.0 release records updated. |
| GitHub CI | Pending | `develop`, `main`, and tag-triggered release workflow must pass. |
| Tag correctness | Pending | `v0.2.0` must point to the validated release source commit. |
| GitHub Release | Pending | Public release must exist at the published tag with verified assets. |
| Clone test | Pending | Fresh public clone from `v0.2.0` must build and pass Debug/Release CTest. |

## Remaining Local Blockers

None recorded.

`GO - ready for public release`
