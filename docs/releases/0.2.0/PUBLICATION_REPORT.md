# Elf3D 0.2.0 Publication Report

Purpose: Record the completed public publication of Elf3D 0.2.0.

Applicable version: 0.2.0

Document status: Post-release publication report.

Last verified implementation commit: `e547f123051ec847bc63a59f4cbc45eaea46cd95`

Implementation source paths: `CMakeLists.txt`, `.github/workflows`,
`scripts/package_release.ps1`, `docs/releases/0.2.0`

Known limitations: SDK packaging, external model corpus validation, and
performance benchmark metrics remain deferred.

Related documents: `RELEASE_CHECKLIST.md`, `VALIDATION_SUMMARY.md`,
`RELEASE_ARTIFACTS.md`, `GITHUB_RELEASE_NOTES.md`

## Publication Date

Published on 2026-06-25.

## Release Source

- Release source commit: `e547f123051ec847bc63a59f4cbc45eaea46cd95`
- Release source branch state: `origin/main`
- Release preparation commit: `43e9b24e4e9eaca4c575e7a56b363d9c59858654`
- Main integration PR: <https://github.com/zavelski/elf3d/pull/2>

## Tag

- Tag: `v0.2.0`
- Annotated tag object: `b19797293f93eee728fd72337bde72973f85a98f`
- Peeled target: `e547f123051ec847bc63a59f4cbc45eaea46cd95`
- Tag annotation: `Elf3D 0.2.0`

The tag was pushed explicitly with `git push origin v0.2.0`. It must not be
moved.

## GitHub Release

- URL: <https://github.com/zavelski/elf3d/releases/tag/v0.2.0>
- Title: `Elf3D 0.2.0`
- Draft: no
- Prerelease: no
- Tag: `v0.2.0`
- Published at: 2026-06-25 09:07:16 UTC

## CI And Release Workflow

| Workflow | Run | Result | Notes |
| --- | --- | --- | --- |
| Develop CI | <https://github.com/zavelski/elf3d/actions/runs/28129043437> | Passed | Debug and Release jobs passed for `43e9b24e4e9eaca4c575e7a56b363d9c59858654`. |
| Main CI | <https://github.com/zavelski/elf3d/actions/runs/28129065207> | Passed | Debug and Release jobs passed for `e547f123051ec847bc63a59f4cbc45eaea46cd95`. |
| Tag release workflow | <https://github.com/zavelski/elf3d/actions/runs/28159209602> | Passed | Release configured, built, tested, packaged, uploaded artifacts, and created the GitHub Release. |

The tag-triggered release workflow job `83395213484` completed successfully in
2m40s.

## Published Assets

| Asset | Size | SHA-256 |
| --- | ---: | --- |
| `elf3d-viewer-0.2.0-windows-x64.zip` | 1,132,689 bytes | `ad2fceae8ff0af7d83521a92e3e1cd20407ac209900ba62b3d8e544fe7ad9021` |
| `SHA256SUMS.txt` | 102 bytes | Contains checksum for the viewer ZIP. |

Downloaded release assets from GitHub and verified `SHA256SUMS.txt` against the
downloaded ZIP. The published ZIP contains `assets/font/DroidSans.ttf`, all
generated toolbar PNG icons, third-party notices, `elf3d_viewer.exe`, and
`elf3d.dll`.

## Public Clone Test

Fresh clone command:

```powershell
git clone --branch v0.2.0 --depth 1 https://github.com/zavelski/elf3d.git out\validation\public-clone-0.2.0
```

The clone checked out peeled tag target
`e547f123051ec847bc63a59f4cbc45eaea46cd95`.

Validation from the public clone:

- `cmake --fresh --preset windows-debug`: passed
- `cmake --build --preset windows-debug --parallel`: passed
- `ctest --preset windows-debug --output-on-failure`: 16/16 passed
- `cmake --fresh --preset windows-release`: passed; the process exceeded the
  initial 10-minute shell timeout but completed and generated the Release build
  tree
- `cmake --build --preset windows-release --parallel`: passed
- `ctest --preset windows-release --output-on-failure`: 16/16 passed

## Known Limitations

- Windows/OpenGL is the only validated platform/backend.
- The public DLL surface is a C++ API and not a stable C ABI.
- SDK packaging is deferred.
- External model corpus validation did not run.
- Performance benchmark metrics are not claimed.
