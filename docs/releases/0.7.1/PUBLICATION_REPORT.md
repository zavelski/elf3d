# Elf3D 0.7.1 Publication Report

Purpose: Record the post-release publication verification for Elf3D 0.7.1.

Applicable version: 0.7.1

Document status: Post-release publication record.

Last verified Git commit: `5fa174b2c688654fd861438c65d4a3d6b5adb09a`

Implementation source paths: remote `main`, remote `develop`, tag `v0.7.1`,
GitHub Release `v0.7.1`, release workflow `28473203548`

Known limitations: This report records publication and process-smoke
verification. It does not add a visible/manual viewer rendering pass.

Related documents: `PROJECT_STATE_EN.md`, `VALIDATION_SUMMARY.md`,
`RELEASE_ARTIFACTS.md`, `RELEASE_CHECKLIST.md`

## Published Source

- Remote `main`: `5fa174b2c688654fd861438c65d4a3d6b5adb09a`
- Remote `develop`: `5fa174b2c688654fd861438c65d4a3d6b5adb09a`
- Published tag: `v0.7.1`
- Peeled tag target: `5fa174b2c688654fd861438c65d4a3d6b5adb09a`

## GitHub Validation

| Area | Result | Evidence |
| --- | --- | --- |
| CI on `develop` | Passed | GitHub Actions run `28473203860` completed successfully. |
| CI on `main` | Passed | GitHub Actions run `28473203611` completed successfully. |
| Release workflow | Passed | GitHub Actions run `28473203548` completed successfully. |
| GitHub Release | Published | [Elf3D 0.7.1](https://github.com/zavelski/elf3d/releases/tag/v0.7.1) is not draft or prerelease. |
| Public clone | Passed | Fresh clone of `v0.7.1` resolved to `5fa174b2c688654fd861438c65d4a3d6b5adb09a`. |

## Published Assets

| Asset | Size | SHA-256 |
| --- | ---: | --- |
| `elf3d-viewer-0.7.1-windows-x64.zip` | 1,503,328 bytes | `d83ad2ff73531ba4786edf7d6e0630ebed639d99bb83f53609b6c653e718633f` |
| `SHA256SUMS.txt` | 102 bytes | `c608aa5e07b8fc8cc0669d31087a54e310356fd2b994df644e2d1c0aa6671cbf` |

The downloaded ZIP checksum matched the published `SHA256SUMS.txt`. The
downloaded ZIP contained 29 files, including the viewer executable, `elf3d.dll`,
assets, root license/readme files, and third-party notices.

The published ZIP checksum differs from the local pre-publication ZIP recorded
in `RELEASE_ARTIFACTS.md` because the GitHub release workflow rebuilt and
repacked the archive on the runner.

## Downloaded Asset Smoke

The downloaded GitHub ZIP was extracted to `out/release-download-0.7.1/verify`.
The packaged `elf3d_viewer.exe` stayed alive for 5 seconds before being stopped.
This was a process smoke only; no visible/manual rendering inspection was
performed.
