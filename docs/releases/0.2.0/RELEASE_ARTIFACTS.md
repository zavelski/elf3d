# Elf3D 0.2.0 Release Artifacts

Purpose: Define and record expected GitHub Release assets for Elf3D 0.2.0.

Applicable version: 0.2.0

Document status: Release artifact validation record.

Last verified implementation commit: pending 0.2.0 release source commit

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
| `elf3d-viewer-0.2.0-windows-x64.zip` | 1,132,433 bytes | `8cdd519ccee832fb8705a307eba32e46759e8920fef37ea2993a0b85b599c3e4` |
| `SHA256SUMS.txt` | 102 bytes | Contains checksum for the viewer ZIP. |

GitHub automatically provides source archives for the `v0.2.0` tag. Duplicate
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
assets/font/DroidSans.ttf
assets/icon/open.png
assets/icon/reload.png
assets/icon/fit_view.png
assets/icon/reset_camera.png
assets/icon/select.png
assets/icon/measure.png
assets/icon/clipping_panel.png
assets/icon/section_plane.png
assets/icon/add_clipping_box.png
assets/icon/clear_clipping.png
assets/icon/hide_selected.png
assets/icon/show_selected.png
assets/icon/isolate_selected.png
assets/icon/show_all.png
assets/icon/reset_layout.png
third_party_licenses/cgltf-LICENSE.txt
third_party_licenses/droidsans-APACHE-2.0.txt
third_party_licenses/glad-LICENSE.txt
third_party_licenses/glfw-LICENSE.md
third_party_licenses/glm-copying.txt
third_party_licenses/imgui-LICENSE.txt
third_party_licenses/stb-LICENSE.txt
```

The viewer package does not include PDB files, object files, CMake
intermediates, generated Visual Studio files, private models, raw build
directories, logs, or `imgui.ini`.

The package README documents the external Microsoft Visual C++ Redistributable
requirement rather than copying redistributable runtime DLLs from a local
developer machine.

## Packaged Viewer Smoke

The final ZIP was extracted under `out/validation/package-0.2.0`. The extracted
`elf3d_viewer.exe` was launched from the extracted package directory without a
model argument, created a window, and exited with code 0 after
`CloseMainWindow()`.

## SDK Package Decision

The SDK package is deferred for 0.2.0. Publishing a partial SDK archive would
overstate current embedding-package support.
