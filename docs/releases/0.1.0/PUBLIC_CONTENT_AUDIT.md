# Elf3D 0.1.0 Public Content Audit

Purpose: Record public-exposure checks before publishing Elf3D 0.1.0 to
`zavelski/elf3d`.

Applicable version: 0.1.0

Document status: Public content audit record.

Last verified implementation commit: `898d60fbd4928111f13f77f6c92c0d8d3c92292b`

Implementation source paths: `README.md`, `LICENSE`, `THIRD_PARTY.md`,
`include/elf3d`, `modules`, `facade/elf3d`, `integrations/imgui`,
`apps/viewer`, `tests`, `third_party`, `docs`

Known limitations: Dedicated secret-scanning tools such as gitleaks and
trufflehog were not available on `PATH`; pattern scans were used instead.

Related documents: `PUBLICATION_PRECHECK.md`, `RELEASE_CHECKLIST.md`,
`../../../THIRD_PARTY.md`, `../0.1.0/KNOWN_LIMITATIONS.md`

## Scope

The audit covered tracked repository files, local branches, local history,
release documentation, public headers, source files, tests, fixtures,
third-party notices, generated GLAD loader files, and ignored local build
outputs.

Ignored `out/` build output and `imgui.ini` runtime state were checked for Git
tracking and confirmed untracked.

## Tools and Commands Used

- `git status --short --ignored`
- `git ls-files`
- `git log --oneline --all`
- `git rev-list --all` with `git grep`
- `rg --files`
- `rg` for targeted public-exposure terms
- PowerShell `Select-String` pattern scans over tracked files

`gitleaks` and `trufflehog` were checked but were not available on `PATH`.

## Secret and Credential Scan

Checked categories:

- password, secret, API key, access token, auth token, GitHub token, client
  secret keywords;
- private-key block markers;
- embedded credentials in URLs;
- GitHub token-shaped strings;
- AWS access-key-shaped strings.

Result:

- no tracked-file matches;
- no local-history matches from `git rev-list --all` plus `git grep`;
- no `.env`, credential, token, private-key, certificate, `.pfx`, or `.p12`
  files were found by filename scan.

## Confidential and Proprietary Content Scan

Checked categories:

- Yulio, CET, Revit, LWNative, customer, confidential, proprietary, private
  company, internal server, and server path terms;
- email address patterns;
- user-specific Windows absolute paths;
- model, asset, fixture, example, screenshot, and demo paths.

Findings:

- One `proprietary` match appears in `ARCHITECTURE.md` as a future plugin use
  case, not as included proprietary content.
- The broad keyword scan produced expected false positives for words such as
  `private` in CMake visibility, implementation boundaries, and documentation.
- The only tracked model fixture is `tests/fixtures/textured_pbr.gltf`; its
  asset generator states it is an Elf3D project-owned validation fixture.
- The `README.md` command example uses `C:\models\scene.glb`; this is a generic
  placeholder path, not a user-specific or private path.

No proprietary customer model, CET export, Revit export, Yulio customer data,
private company identifier, private email address, internal server path, or
confidential model fixture was found.

## Dependency and License Scan

Third-party components are documented in `THIRD_PARTY.md`:

| Dependency | Source form | License notice |
| --- | --- | --- |
| Dear ImGui | FetchContent from pinned official commit | `third_party/licenses/imgui-LICENSE.txt` |
| GLFW | FetchContent from pinned official commit | `third_party/licenses/glfw-LICENSE.md` |
| GLM | FetchContent from pinned official commit archive | `third_party/licenses/glm-copying.txt` |
| GLAD | Generated C loader checked into `third_party/glad` | `third_party/licenses/glad-LICENSE.txt` |
| cgltf | FetchContent from pinned official commit | `third_party/licenses/cgltf-LICENSE.txt` |
| stb | FetchContent from pinned official commit | `third_party/licenses/stb-LICENSE.txt` |

The root Elf3D project license is MIT in `LICENSE`. Third-party notices remain
separate and were not removed, replaced, or reinterpreted as Elf3D source code.

## Tracked-File Hygiene

Confirmed:

- `out/` is ignored and untracked;
- `.vs/` is ignored;
- `CMakeUserPresets.json` is ignored;
- `imgui.ini` is ignored and untracked;
- no tracked PDB, object, import-library, executable, DLL, CMake cache, Visual
  Studio solution/project, crash dump, temporary log, release archive, or
  package archive was found by filename scan.

`.gitattributes` is not currently present. Add one before publication if final
line-ending policy should be enforced by Git rather than developer defaults.

## Large Files

No tracked file larger than 1 MiB was found. The largest tracked files are
project source, documentation, and the checked-in generated GLAD loader.

Git LFS is not required for the current tracked repository content.

## Files Excluded or Replaced

No tracked confidential files required removal or replacement during this audit.

## Remaining Blockers

Content blockers:

- none found in the scanned repository content.

Publication blockers outside content scope remain recorded in
`PUBLICATION_PRECHECK.md`, including missing `main`, missing `v0.1.0`, missing
remote, missing CI/release packaging validation, and incomplete manual visual
viewer validation.
