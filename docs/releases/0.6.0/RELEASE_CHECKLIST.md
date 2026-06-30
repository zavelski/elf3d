# Elf3D 0.6.0 Release Checklist

Purpose: Record the release-readiness decision for Elf3D 0.6.0.

Applicable version: 0.6.0

Document status: Local release checklist.

Release source identifier: local tag `v0.6.0` after release commit.

Implementation source paths: `CMakeLists.txt`, `CMakePresets.json`,
`.github/workflows`, `scripts/package_release.ps1`, `include/elf3d`, `modules`,
`facade/elf3d`, `apps/viewer`, `tests`, `third_party`, `LICENSE`,
`THIRD_PARTY.md`, `docs`

Known limitations: Remaining limitations and publication blockers are recorded
in `KNOWN_LIMITATIONS.md` and below.

Related documents: `PROJECT_STATE_EN.md`, `VALIDATION_SUMMARY.md`,
`KNOWN_LIMITATIONS.md`, `PUBLIC_CONTENT_AUDIT.md`, `RELEASE_ARTIFACTS.md`

## Decision

`LOCAL-GO - local source/package validated; publication deferred`

## Checklist

| Gate | Status | Evidence |
| --- | --- | --- |
| Public content audit | Passed locally | Origin is public, conflicting tag/release were absent, tracked generated-artifact scan was empty, and no secret pattern blocker was found. |
| Project license | Passed | Root `LICENSE` is MIT. |
| Third-party licenses | Passed | `THIRD_PARTY.md` records pinned dependencies; notices are preserved inside each `third_party/<name>/` subtree and copied into release packages. |
| Version consistency | Passed | CMake, runtime data, both version tests, docs, workflow, package, and release notes use `0.6.0`; no stale previous-development-version references remain. |
| Debug configure | Passed | Fresh `windows-debug` configure. |
| Debug build | Passed | All Debug targets built. |
| Debug tests | Passed | 17/17 passed. |
| Release configure | Passed | Fresh `windows-release` configure. |
| Release build | Passed | All Release targets built. |
| Release tests | Passed | 17/17 passed. |
| Release archive inspection | Passed | Required runtime files, assets, and notices present; forbidden build outputs absent. |
| Checksums | Passed | ZIP SHA-256 is `1aa9b3e8415bfb0c5368df27808d69c1d8272a1221f37a1df0a3375bfa247861`. |
| Complete manual viewer validation | Not run | Not performed in this local preparation. |
| SDK package | Not applicable | Deferred; no SDK archive is produced. |
| Documentation | Passed locally | Living docs and 0.6.0 release records updated. |
| Local tag target | Passed locally | The validated source is intended to be tagged `v0.6.0`; tag creation is external Git metadata performed after committing the source. |
| GitHub CI | Not run | No push is performed. |
| GitHub Release | Not run | No GitHub Release is created. |
| Public clone test | Not run | Requires a published immutable tag. |

## Publication Follow-up

- Push synchronized `main` and `develop` only after local review.
- Push the local annotated `v0.6.0` tag when ready to publish.
- Require green GitHub CI before treating the release as public.
- Verify tag-triggered release workflow, published checksums, and a fresh public
  clone after publication.
