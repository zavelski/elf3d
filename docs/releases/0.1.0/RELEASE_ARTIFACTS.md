# Elf3D 0.1.0 Release Artifacts

Purpose: Define and record the expected GitHub Release assets for Elf3D 0.1.0.

Applicable version: 0.1.0

Document status: Release artifact validation record.

Last verified implementation commit before final package-record update:
`a99bb1008882994d3127141019b049927cbc2c97`

Implementation source paths: `scripts/package_release.ps1`,
`.github/workflows/release.yml`, `apps/viewer`, `facade/elf3d`, `LICENSE`,
`THIRD_PARTY.md`, `third_party/licenses`

Known limitations: SDK packaging is deferred because the repository does not
yet provide install rules, exported CMake package files, or an external
consumer validation workflow.

Related documents: `GITHUB_RELEASE_NOTES.md`, `RELEASE_CHECKLIST.md`,
`VALIDATION_SUMMARY.md`

## Assets Created Locally

| Asset | Size | SHA-256 |
| --- | ---: | --- |
| `elf3d-viewer-0.1.0-windows-x64.zip` | 923,630 bytes | `1d39c50460e86083f448557ed6a7eddad3974d26b99e84e4c2cfc030c5265c92` |
| `SHA256SUMS.txt` | 102 bytes | Contains checksum for the viewer ZIP. |

GitHub automatically provides source archives for the `v0.1.0` tag. Duplicate
source archives are not committed to Git and are not produced by the packaging
script.

## Viewer Package Contents

Inspected ZIP contents:

```text
elf3d_viewer.exe
elf3d.dll
LICENSE
README.txt
THIRD_PARTY.md
third_party_licenses/cgltf-LICENSE.txt
third_party_licenses/glad-LICENSE.txt
third_party_licenses/glfw-LICENSE.md
third_party_licenses/glm-copying.txt
third_party_licenses/imgui-LICENSE.txt
third_party_licenses/stb-LICENSE.txt
```

The viewer package does not include PDB files, object files, CMake
intermediates, generated Visual Studio files, private models, or raw build
directories.

The package README documents the external Microsoft Visual C++ Redistributable
requirement rather than copying redistributable runtime DLLs from a local
developer machine.

## Packaged Viewer Smoke

The final ZIP was extracted under `out/validation/package-final-run`. The
extracted `elf3d_viewer.exe` was launched from the extracted package directory
without a model argument, created a window, and exited with code 0 after
`CloseMainWindow()`.

Running the extracted viewer created a local `imgui.ini` in the extracted
directory. That file was not present in the ZIP.

## SDK Package Decision

The SDK package is deferred for 0.1.0. Although the build produces public
headers, `elf3d.dll`, and an import library, the repository does not yet provide
validated install/export rules, CMake package metadata, or a minimal external
consumer project. Publishing a partial SDK archive would overstate the current
embedding support.
