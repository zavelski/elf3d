# Elf3D 0.2.0 Public Content Audit

Purpose: Record public-exposure checks before publishing Elf3D 0.2.0.

Applicable version: 0.2.0

Document status: Release audit record.

Last verified implementation commit: pending 0.2.0 release source commit

Implementation source paths: repository root, `apps/viewer/assets`,
`third_party/licenses`, `.github/workflows`, `scripts/package_release.ps1`

Known limitations: Secret scanning is pattern-based and can produce false
positives.

Related documents: `RELEASE_CHECKLIST.md`, `RELEASE_ARTIFACTS.md`,
`../../../THIRD_PARTY.md`

## Checks

| Area | Result | Evidence |
| --- | --- | --- |
| Origin | Passed | `origin` is `https://github.com/zavelski/elf3d.git`. |
| Conflicting tag | Passed | `v0.2.0` was absent during preflight. |
| Conflicting GitHub Release | Passed | `gh release view v0.2.0` returned `release not found`. |
| Unsafe content scan | Passed with false positives | Pattern scan only matched source variable names such as `engine_token`; no credentials or private keys were found. |
| Viewer icons | Passed | Toolbar PNGs are generated original assets, not copied LWApp PNG files. |
| Droid Sans provenance | Passed | `apps/viewer/assets/font/DroidSans.ttf` is byte-identical to AOSP revision `dba35c0` and is recorded under Apache-2.0. |
| Third-party notices | Passed | `THIRD_PARTY.md` and `third_party/licenses/` include notices for runtime and build dependencies. |
| Build outputs | Passed | Build outputs and `imgui.ini` remain ignored and are not release-source files. |

## Follow-up

Repeat the unsafe-content scan before the final release-source commit if
additional files are added.
