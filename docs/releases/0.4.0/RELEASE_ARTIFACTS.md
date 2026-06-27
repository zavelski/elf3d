# Elf3D 0.4.0 Release Artifacts

Purpose: Define and record expected release assets for Elf3D 0.4.0.

Applicable version: 0.4.0

Document status: Local release artifact validation record.

Last verified implementation commit: pending 0.4.0 release source commit

Implementation source paths: `scripts/package_release.ps1`,
`.github/workflows/release.yml`, `apps/viewer`, `facade/elf3d`, `LICENSE`,
`THIRD_PARTY.md`, `third_party/licenses`

Known limitations: SDK packaging is deferred because the repository does not
provide install rules, exported CMake package files, or external consumer
validation. No 0.4.0 assets have been published on GitHub.

Related documents: `GITHUB_RELEASE_NOTES.md`, `RELEASE_CHECKLIST.md`,
`VALIDATION_SUMMARY.md`

## Assets Created Locally

| Asset | Size | SHA-256 |
| --- | ---: | --- |
| `elf3d-viewer-0.4.0-windows-x64.zip` | 1,475,991 bytes | `160faae3e52d1f83514097a3a90bb42a011eab4f984b10418561f64ac7a4d266` |
| `SHA256SUMS.txt` | 102 bytes | Contains the matching viewer ZIP checksum. |

## Viewer Package Contents

The ZIP contains 28 entries. Inspection confirmed:

```text
elf3d_viewer.exe
elf3d.dll
LICENSE
README.txt
THIRD_PARTY.md
assets/font/DroidSans.ttf
assets/icon/*.png
third_party_licenses/*
```

The package contains no PDB, object, module BMI/IFC, CMake intermediate, log,
dump, or `imgui.ini` files.

## Packaged Viewer Smoke

The ZIP was extracted under
`out/validation/package-0.4.0-release-check`. The extracted viewer was started
with `tests/fixtures/textured_pbr.gltf`, created a native window, accepted
`CloseMainWindow()`, and exited with code 0. This is process smoke only, not a
visual or interaction validation.

## Published Assets

None. GitHub publication was explicitly excluded from this local preparation.
