# Elf3D 0.7.1 Release Artifacts

Purpose: Define and record expected release assets for Elf3D 0.7.1.

Applicable version: 0.7.1

Document status: Local release artifact validation record.

Last verified Git commit: local tag `v0.7.1` after release commit

Implementation source paths: `scripts/package_release.ps1`,
`.github/workflows/release.yml`, `apps/viewer`, `facade/elf3d`, `LICENSE`,
`THIRD_PARTY.md`, `third_party`

Known limitations: SDK packaging is deferred because the repository does not
provide install rules, exported CMake package files, or external consumer
validation. No 0.7.1 assets have been published on GitHub.

Related documents: `GITHUB_RELEASE_NOTES.md`, `RELEASE_CHECKLIST.md`,
`VALIDATION_SUMMARY.md`

## Assets Created Locally

| Asset | Size | SHA-256 |
| --- | ---: | --- |
| `elf3d-viewer-0.7.1-windows-x64.zip` | 1,503,325 bytes | `3e05fbd8bcb554ea95392762257cc62c72f52714d2450da8bc237c6ed4428477` |
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

Passed locally as a process smoke. The ZIP was extracted to
`out/release/verify-0.7.1-final`; the packaged `elf3d_viewer.exe` stayed alive
for 5 seconds before being stopped. No visible/manual rendering inspection was
performed.

## Published Assets

None. GitHub publication is explicitly excluded from this local preparation.
