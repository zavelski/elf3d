# Elf3D 0.1.0 Publication Report

Purpose: Record the completed public publication of Elf3D 0.1.0.

Applicable version: 0.1.0

Document status: Post-publication report.

Publication date: 2026-06-24

Public repository: `https://github.com/zavelski/elf3d`

Repository state:

- Visibility: public.
- Default branch: `main`.
- Description: `A modular C++20 3D visualization engine for glTF 2.0 desktop applications.`
- Topics: `cpp`, `cpp20`, `gltf`, `glb`, `opengl`, `3d-engine`,
  `3d-visualization`, `bim`, `imgui`, `graphics`.
- Authenticated publication account: `zavelski`.
- Local GitHub CLI version used for verification: `gh` 2.95.0.

## Branches and Tag

Remote `main` commit:
`53047abef3f7e7c31d82913c1e9642d5f1b0d294`

Remote `develop` release baseline before this post-publication report:
`c6f0d743432b2efdbc45bec15f0427bf00a901f4`

`develop` was fast-forwarded to the released `main` commit before this
post-publication report:
`53047abef3f7e7c31d82913c1e9642d5f1b0d294`

The final remote `develop` commit is the post-publication documentation commit
that contains this report.

Annotated tag:

- Tag: `v0.1.0`
- Tag object: `24c3357ab0ae4aa20bc6be8d3de6403e30158e00`
- Tag target: `53047abef3f7e7c31d82913c1e9642d5f1b0d294`
- Tag annotation: `Elf3D 0.1.0 — first audited public visualization engine baseline`

The tag was pushed explicitly with `git push origin v0.1.0`. No bulk tag push
or force push was used.

## GitHub Actions

Corrected branch CI:

- `develop` CI run: `https://github.com/zavelski/elf3d/actions/runs/28083541505`
- `main` CI run for the final release commit:
  `https://github.com/zavelski/elf3d/actions/runs/28084258969`
- Runner image: `windows-2022`.
- Checkout action: `actions/checkout@v6`.
- Debug job: passed configure, build, and 16/16 CTest tests.
- Release job: passed configure, build, and 16/16 CTest tests.

Tag-triggered release workflow:

- Release run: `https://github.com/zavelski/elf3d/actions/runs/28084521674`
- Head branch/tag: `v0.1.0`
- Head SHA: `53047abef3f7e7c31d82913c1e9642d5f1b0d294`
- Job: `Windows viewer package`
- Result: success.
- Runner image: `windows-2022`.
- Checkout action: `actions/checkout@v6`.
- Release configure/build/test/package/upload steps passed.
- Release tests passed 16/16.
- `Create GitHub Release` step passed.

## GitHub Release

Release URL:
`https://github.com/zavelski/elf3d/releases/tag/v0.1.0`

Release state:

- Title: `Elf3D 0.1.0`.
- Tag: `v0.1.0`.
- Target commitish: `main`.
- Draft: false.
- Prerelease: false.
- Latest release: yes.
- Published at: `2026-06-24T08:09:55Z`.

Uploaded assets:

| Asset | Size | SHA-256 |
| --- | ---: | --- |
| `elf3d-viewer-0.1.0-windows-x64.zip` | 923,882 bytes | `8eabbe62bf8aaf7a0209b22196654b5a725fe83e9b111ac48093daa2b1ea3507` |
| `SHA256SUMS.txt` | 102 bytes | `f934ed9e9cb4d6cca1fe44712a727de7565f0323c732aa6b3271b86f24e22c81` |

Downloaded release assets were verified locally. The downloaded
`SHA256SUMS.txt` contained:

```text
8eabbe62bf8aaf7a0209b22196654b5a725fe83e9b111ac48093daa2b1ea3507  elf3d-viewer-0.1.0-windows-x64.zip
```

The downloaded ZIP hash matched that checksum. No SDK archive was uploaded for
0.1.0 because SDK packaging is deferred and no SDK archive was validated.

The locally regenerated pre-publication package had a different ZIP checksum:

```text
1d39c50460e86083f448557ed6a7eddad3974d26b99e84e4c2cfc030c5265c92  elf3d-viewer-0.1.0-windows-x64.zip
```

The published GitHub Release asset checksum above is the release distribution
source of truth.

## Fresh Public Clone Test

The public clone test was run from a new clone of:

```text
https://github.com/zavelski/elf3d.git
```

Validated checkout:

- Tag: `v0.1.0`.
- Detached `HEAD`: `53047abef3f7e7c31d82913c1e9642d5f1b0d294`.
- Peeled tag target: `53047abef3f7e7c31d82913c1e9642d5f1b0d294`.
- Submodule initialization command completed; no submodules were required.

Validation commands:

```powershell
cmake --fresh --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug --output-on-failure
cmake --fresh --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release --output-on-failure
```

Result:

- Debug configure/build passed.
- Debug CTest passed 16/16.
- Release configure/build passed.
- Release CTest passed 16/16.
- No files from the original development checkout were required.

The first clone-test attempt under `Z:\Elf3D\out\validation` was discarded
because Git refused operations on that ownershipless filesystem with the
`dubious ownership` safety guard. No global `safe.directory` exception was
added. The successful clone test was rerun under the Windows user temp tree.
MSBuild emitted `MSB8029` warnings because build output directories were under
`%TEMP%`; those warnings were inspected and were path-related, not project
compile diagnostics.

## Remaining Limitations

- Manual viewer interaction validation is user-performed, not automated.
- SDK packaging remains deferred for 0.1.0.
- No external model corpus was validated.
- No benchmark or performance metrics are claimed for 0.1.0.

## Recommended Manual Settings

- Enable branch protection for `main` with required CI before future releases.
- Protect release tags or restrict tag deletion/rewrite for `v*` tags.
- Keep repository visibility public and default branch `main`.
- Keep GitHub Actions enabled for branch CI and tag-triggered releases.
