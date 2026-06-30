# Elf3D 0.7.1 Public Content Audit

Purpose: Record public-exposure checks before publishing Elf3D 0.7.1.

Applicable version: 0.7.1

Document status: Local release audit record.

Last verified Git commit: local tag `v0.7.1` after release commit

Implementation source paths: repository root, `apps/viewer/assets`,
`third_party`, `.github/workflows`, `scripts/package_release.ps1`

Known limitations: Secret scanning is pattern-based and can produce false
positives. GitHub remote state is not changed by this local audit.

Related documents: `RELEASE_CHECKLIST.md`, `RELEASE_ARTIFACTS.md`,
`../../../THIRD_PARTY.md`

## Checks

| Area | Result | Evidence |
| --- | --- | --- |
| Origin | Passed | `origin` is `https://github.com/zavelski/elf3d.git`. |
| Conflicting local tag | Passed | Local `v0.7.1` was absent during preflight. |
| Conflicting remote tag | Passed | `git ls-remote --tags origin refs/tags/v0.7.1 refs/tags/v0.7.1^{}` returned no entries. |
| Conflicting GitHub Release | Passed | `gh release view v0.7.1 --repo zavelski/elf3d` reported `release not found`. |
| Unsafe content scan | Passed locally | No hits were found for the local secret-pattern scan excluding `.git/` and `out/`. |
| Third-party notices | Passed | `THIRD_PARTY.md` records the dependencies, and each vendored product subtree preserves its own notice file. |
| Build outputs | Passed | Tracked-file scan found no `out/`, `build/`, object, PDB, IFC, or MSVC intermediate artifacts. |
| Public API boundary | Passed | Scan of public facade/include headers found no native GL, GLFW, Dear ImGui, GLM, cgltf, or GLAD types. |

## Result

No local public-content blocker was found. Repeat the scan if the release
source changes before publication.
