# Elf3D 0.7.2 Release Checklist

Purpose: Record the release-readiness decision for Elf3D 0.7.2.

Applicable version: 0.7.2

Document status: Local release checklist.

Last verified Git commit: local tag `v0.7.2` after release commit

Implementation source paths: `CMakeLists.txt`, `CMakePresets.json`,
`.github/workflows`, `scripts/package_release.ps1`, `include/elf3d`, `modules`,
`facade/elf3d`, `apps/viewer`, `tests`, `third_party`, `LICENSE`,
`THIRD_PARTY.md`, `docs`

Known limitations: Remaining limitations and publication blockers are recorded
in `KNOWN_LIMITATIONS.md` and below.

Related documents: `PROJECT_STATE_EN.md`, `VALIDATION_SUMMARY.md`,
`KNOWN_LIMITATIONS.md`, `PUBLIC_CONTENT_AUDIT.md`, `RELEASE_ARTIFACTS.md`

## Decision

`LOCAL-GO_WITH_LIMITATION - automated source/package validation passed; visible manual viewer, GitHub, and public clone validation remain deferred`

## Checklist

| Gate | Status | Evidence |
| --- | --- | --- |
| Public content audit | Passed locally | Origin is public, conflicting tag/release were absent, tracked generated-artifact scan was empty, and no secret pattern blocker was found. |
| Project license | Passed | Root `LICENSE` is MIT. |
| Third-party licenses | Passed | `THIRD_PARTY.md` records pinned dependencies; notices are preserved inside each `third_party/<name>/` subtree and copied into release packages. |
| Version consistency | Passed | CMake, runtime data, both version tests, and 0.7.2 release notes use `0.7.2`; the package script and release workflow derive the release version from CMake. |
| Debug configure | Passed | Fresh `windows-debug` configure. |
| Debug build | Passed | All Debug targets built. |
| Debug tests | Passed | 18/18 passed, including `elf3d.opengl_render_smoke` on a real hidden OpenGL context. |
| Release configure | Passed | Fresh `windows-release` configure. |
| Release build | Passed | All Release targets built. |
| Release tests | Passed | 18/18 passed, including `elf3d.opengl_render_smoke` on a real hidden OpenGL context. |
| Release archive inspection | Passed | The packaged ZIP contains 29 entries, including runtime binaries, assets, licenses, and third-party notices, with build artifacts absent. |
| Checksums | Passed | `SHA256SUMS.txt` contains the matching ZIP SHA-256 `a20d8ad1483c9237a9536b29e6c327398a74a2e7a57cef07fd42da535d1c96c4`. |
| Deterministic package metadata | Passed | Two local package runs against the same staged Release binaries produced the same ZIP SHA-256. |
| Targeted manual viewer validation | Not run | No visible/manual viewer interaction pass was performed after the 0.7.2 fixes. |
| Packaged viewer smoke | Passed locally | Extracted ZIP viewer process stayed alive for 5 seconds before being stopped; this did not include visual rendering inspection. |
| SDK package | Not applicable | Deferred; no SDK archive is produced. |
| Documentation | Passed locally | Living docs and 0.7.2 release records updated. |
| Local tag target | Passed locally | The validated source is intended to be tagged `v0.7.2`; tag creation is external Git metadata performed after committing the source. |
| GitHub CI | Not run | No push is performed. |
| GitHub Release | Not run | No GitHub Release is created. |
| Public clone test | Not run | Requires a published immutable tag. |

## Publication Follow-up

- Push synchronized `main` and `develop` only after local review.
- Push the local annotated `v0.7.2` tag when ready to publish.
- Require green GitHub CI before treating the release as public.
- Verify tag-triggered release workflow, published checksums, and a fresh public
  clone after publication.
