# Elf3D 0.4.0 Release Checklist

Purpose: Record the release-readiness decision for Elf3D 0.4.0.

Applicable version: 0.4.0

Document status: Local release checklist.

Last verified implementation commit: pending 0.4.0 release source commit

Implementation source paths: `CMakeLists.txt`, `CMakePresets.json`,
`.github/workflows`, `scripts/package_release.ps1`, `include/elf3d`, `modules`,
`facade/elf3d`, `apps/viewer`, `tests`, `LICENSE`, `THIRD_PARTY.md`, `docs`

Known limitations: Remaining limitations and publication blockers are recorded
in `KNOWN_LIMITATIONS.md` and below.

Related documents: `PROJECT_STATE_EN.md`, `VALIDATION_SUMMARY.md`,
`KNOWN_LIMITATIONS.md`, `PUBLIC_CONTENT_AUDIT.md`, `RELEASE_ARTIFACTS.md`

## Decision

`NO-GO - publication blocked`

## Checklist

| Gate | Status | Evidence |
| --- | --- | --- |
| Public content audit | Passed locally | Pattern scan found no secrets; tracked generated-artifact scan was empty. |
| Project license | Passed | Root `LICENSE` is MIT. |
| Third-party licenses | Passed | `THIRD_PARTY.md` and all required files under `third_party/licenses/` exist. |
| Version consistency | Passed | CMake, runtime data, both version tests, docs, workflow, package, and release notes use `0.4.0`. |
| Debug configure | Passed | Fresh `windows-debug` configure. |
| Debug build | Passed | All targets built with `/W4 /WX`. |
| Debug tests | Passed | Final run passed 17/17 after correcting the smoke-test version expectation. |
| Release configure | Passed | Fresh `windows-release` configure. |
| Release build | Passed | All targets built with `/W4 /WX`. |
| Release tests | Passed | 17/17 passed. |
| Viewer process smoke | Passed | Extracted package viewer created a window and exited with code 0. |
| Complete manual viewer validation | Blocked | Not performed in this local preparation; About was intentionally not rechecked. |
| Release archive inspection | Passed | Required runtime files, assets, and notices present; forbidden build outputs absent. |
| Checksums | Passed | ZIP SHA-256 is `160faae3e52d1f83514097a3a90bb42a011eab4f984b10418561f64ac7a4d266`. |
| SDK package | Not applicable | Deferred; no SDK archive is produced. |
| Documentation | Passed locally | Living docs and 0.4.0 release records updated. |
| GitHub CI | Blocked | No push was performed. |
| Tag correctness | Blocked | `v0.4.0` was intentionally not created or published. |
| GitHub Release | Blocked | No GitHub Release was created. |
| Public clone test | Blocked | Requires a published immutable tag. |

## Publication Blockers

- Complete the manual viewer interaction and visual checklist.
- Push synchronized release-source branches and require green CI.
- Create and verify the annotated `v0.4.0` tag on the validated source commit.
- Verify the tag-triggered release workflow and published checksums.
- Build and test a fresh public clone from the published tag.

`NO-GO - publication blocked`
