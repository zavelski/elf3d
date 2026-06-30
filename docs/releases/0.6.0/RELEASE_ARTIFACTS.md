# Elf3D 0.6.0 Release Artifacts

Purpose: Define and record expected release assets for Elf3D 0.6.0.

Applicable version: 0.6.0

Document status: Local release artifact validation record.

Release source identifier: local tag `v0.6.0` after release commit.

Implementation source paths: `scripts/package_release.ps1`,
`.github/workflows/release.yml`, `apps/viewer`, `facade/elf3d`, `LICENSE`,
`THIRD_PARTY.md`, `third_party`

Known limitations: SDK packaging is deferred because the repository does not
provide install rules, exported CMake package files, or external consumer
validation. No 0.6.0 assets have been published on GitHub.

Related documents: `GITHUB_RELEASE_NOTES.md`, `RELEASE_CHECKLIST.md`,
`VALIDATION_SUMMARY.md`

## Assets Created Locally

| Asset | Size | SHA-256 |
| --- | ---: | --- |
| `elf3d-viewer-0.6.0-windows-x64.zip` | 1,502,327 bytes | `1aa9b3e8415bfb0c5368df27808d69c1d8272a1221f37a1df0a3375bfa247861` |
| `SHA256SUMS.txt` | 102 bytes | Contains the matching viewer ZIP checksum. |

## Viewer Package Contents

The ZIP contains 29 entries. Inspection confirmed:

```text
elf3d_viewer.exe
elf3d.dll
LICENSE
README.txt
THIRD_PARTY.md
assets/font/DroidSans.ttf
assets/font/DroidSans-LICENSE-APACHE-2.0.txt
assets/icon/*.png
third_party_licenses/*
```

The package must not contain PDB, object, module BMI/IFC, CMake intermediate,
log, dump, or `imgui.ini` files.

## Packaged Viewer Smoke

Not run in this local preparation. Complete viewer interaction and visual
validation remain manual follow-up work.

## Published Assets

None. GitHub publication is explicitly excluded from this local preparation.
