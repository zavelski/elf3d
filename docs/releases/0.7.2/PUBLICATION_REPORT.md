# Elf3D 0.7.2 Publication Report

Purpose: Record the post-release publication verification for Elf3D 0.7.2.

Applicable version: 0.7.2

Document status: Post-release publication record.

Last verified Git commit: `cf57a46e6a1f06078270aeded6a27e8d36ede09c`

Implementation source paths: remote `main`, remote `develop`, tag `v0.7.2`,
GitHub Release `v0.7.2`, release workflow `28474936710`

Known limitations: This report records publication and process-smoke
verification. It does not add a visible/manual viewer rendering pass.

Related documents: `PROJECT_STATE_EN.md`, `VALIDATION_SUMMARY.md`,
`RELEASE_ARTIFACTS.md`, `RELEASE_CHECKLIST.md`

## Published Source

- Remote `main`: `cf57a46e6a1f06078270aeded6a27e8d36ede09c`
- Remote `develop`: `cf57a46e6a1f06078270aeded6a27e8d36ede09c`
- Published tag: `v0.7.2`
- Peeled tag target: `cf57a46e6a1f06078270aeded6a27e8d36ede09c`

## GitHub Validation

| Area | Result | Evidence |
| --- | --- | --- |
| CI on `develop` | Passed | GitHub Actions run `28474936719` completed successfully. |
| CI on `main` | Passed | GitHub Actions run `28474936994` completed successfully. |
| Release workflow | Passed | GitHub Actions run `28474936710` completed successfully. |
| GitHub Release | Published | [Elf3D 0.7.2](https://github.com/zavelski/elf3d/releases/tag/v0.7.2) is not draft or prerelease. |
| Public clone | Passed | Fresh clone of `v0.7.2` resolved to `cf57a46e6a1f06078270aeded6a27e8d36ede09c`. |

## Published Assets

| Asset | Size | SHA-256 |
| --- | ---: | --- |
| `elf3d-viewer-0.7.2-windows-x64.zip` | 1,503,499 bytes | `4a6e14614f3ff12c69d63cf145ca4c127cd0b414b9e25a214964824693857c8e` |
| `SHA256SUMS.txt` | 102 bytes | `31ddcfd760601bd67ab5b010b4a76bbb894155dd164d454ed842cfa48e449e68` |

The downloaded ZIP checksum matched the published `SHA256SUMS.txt`. The
downloaded ZIP contained 29 files, including the viewer executable, `elf3d.dll`,
assets, root license/readme files, and third-party notices.

The published ZIP checksum differs from the local pre-publication ZIP recorded
in `RELEASE_ARTIFACTS.md` because the GitHub release workflow rebuilt the
Release binaries on the runner. The 0.7.2 package script is deterministic for a
fixed staged file set, but MSVC binary reproducibility across machines is not
claimed.

## Downloaded Asset Smoke

The downloaded GitHub ZIP was extracted to `out/release-download-0.7.2/verify`.
The packaged `elf3d_viewer.exe` stayed alive for 5 seconds before being stopped.
This was a process smoke only; no visible/manual rendering inspection was
performed.
