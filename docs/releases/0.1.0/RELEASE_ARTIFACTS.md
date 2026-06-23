# Elf3D 0.1.0 Release Artifacts

Purpose: Define and record the expected GitHub Release assets for Elf3D 0.1.0.

Applicable version: 0.1.0

Document status: Release artifact plan and validation record.

Last verified implementation commit: pending final validation.

Implementation source paths: `scripts/package_release.ps1`,
`.github/workflows/release.yml`, `apps/viewer`, `facade/elf3d`, `LICENSE`,
`THIRD_PARTY.md`, `third_party/licenses`

Known limitations: SDK packaging is deferred because the repository does not
yet provide install rules, exported CMake package files, or an external
consumer validation workflow.

Related documents: `GITHUB_RELEASE_NOTES.md`, `RELEASE_CHECKLIST.md`,
`VALIDATION_SUMMARY.md`

## Planned Assets

| Asset | Status | Description |
| --- | --- | --- |
| `elf3d-viewer-0.1.0-windows-x64.zip` | Planned | Windows x64 viewer package. |
| `SHA256SUMS.txt` | Planned | SHA-256 checksum file for release assets. |
| `elf3d-sdk-0.1.0-windows-x64.zip` | Deferred | Not produced for 0.1.0. |

GitHub automatically provides source archives for the `v0.1.0` tag. Duplicate
source archives are not committed to Git and are not produced by the packaging
script.

## Viewer Package Contents

Expected staged contents:

```text
elf3d_viewer.exe
elf3d.dll
LICENSE
THIRD_PARTY.md
README.txt
third_party_licenses/
```

The viewer package does not include PDB files, object files, CMake
intermediates, generated Visual Studio files, private models, or raw build
directories.

The package README documents the external Microsoft Visual C++ Redistributable
requirement rather than copying redistributable runtime DLLs from a local
developer machine.

## SDK Package Decision

The SDK package is deferred for 0.1.0. Although the build produces public
headers, `elf3d.dll`, and an import library, the repository does not yet provide
validated install/export rules, CMake package metadata, or a minimal external
consumer project. Publishing a partial SDK archive would overstate the current
embedding support.

## Local Validation

Pending final release validation.
