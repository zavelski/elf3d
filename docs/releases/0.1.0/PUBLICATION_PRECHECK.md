# Elf3D 0.1.0 Publication Precheck

Purpose: Record the local repository state before any publication to
`zavelski/elf3d`.

Applicable version: 0.1.0

Document status: Publication precheck record.

Last verified implementation commit: `898d60fbd4928111f13f77f6c92c0d8d3c92292b`

Implementation source paths: `CMakeLists.txt`, `CMakePresets.json`,
`README.md`, `LICENSE`, `THIRD_PARTY.md`, `include/elf3d`, `modules`,
`facade/elf3d`, `integrations/imgui`, `apps/viewer`, `tests`, `docs`

Known limitations: This record was created before public publication, before
final release validation, before creating `main`, before creating `v0.1.0`, and
before configuring a GitHub remote.

Related documents: `PUBLIC_CONTENT_AUDIT.md`, `RELEASE_CHECKLIST.md`,
`VALIDATION_SUMMARY.md`, `KNOWN_LIMITATIONS.md`, `../../../PROJECT_STATE_EN.md`

## Repository Root

`Z:/Elf3D`

## Current Branch

`audit/0.1.0`

## Branch State

| Branch | State |
| --- | --- |
| `audit/0.1.0` | Present at `898d60fbd4928111f13f77f6c92c0d8d3c92292b`. |
| `develop` | Present at `f8fe3a827bc81dadb461e58bdbe846958dab346a`. |
| `main` | Missing locally at precheck time. |

`main` and `develop` could not be compared because `main` does not exist
locally. `audit/0.1.0` is ahead of `develop` and contains the audited release
candidate documentation and remediation commits.

## Tag State

`v0.1.0` is missing locally at precheck time.

No tag object, annotation, target commit, or publication status exists yet.
Create the tag only after final validation produces a `GO` decision.

## Working Tree State

Initial precheck began with a clean worktree on `audit/0.1.0`. The following
public-release preparation changes were then made locally:

- added root `LICENSE` with standard MIT License text;
- added `ELF3D_LICENSE_SPDX` set to `MIT`;
- updated `README.md` to identify the Elf3D project license as MIT;
- updated `THIRD_PARTY.md` to keep third-party licenses separate from the Elf3D
  project license.

These changes must be committed before final publication validation.

## Remote State

No Git remotes are configured at precheck time.

`origin` must point exactly to `zavelski/elf3d` before any push.

## Repository Size and Storage

- Loose objects: 2.82 MiB.
- Packed objects: none at precheck time.
- No tracked file larger than 1 MiB was found.
- No Git submodules are configured.
- Git LFS is installed/configured globally, but no `.gitattributes` LFS tracking
  rules are committed in this repository.

## License State

The project license is MIT, authorized for this release with copyright:

`Copyright (c) 2026 Serge Zavelski`

The root `LICENSE` file contains the standard MIT License text. `README.md`
identifies the project license as MIT. `THIRD_PARTY.md` preserves third-party
license separation. No source-file license-header convention is currently used.

## Required Documents Located

Located during precheck:

- `PROJECT_STATE_EN.md`
- `CHANGELOG.md`
- `README.md`
- `LICENSE`
- `THIRD_PARTY.md`
- `AGENTS.md`
- `CODING_POLICY.md`
- `ARCHITECTURE.md`
- `docs/`
- `docs/releases/0.1.0/`
- existing audit, remediation, validation, and release-candidate records

## Publication Blockers

Current blockers before public publication:

- local `main` branch is missing;
- local annotated `v0.1.0` tag is missing;
- no GitHub remote is configured;
- GitHub repository `zavelski/elf3d` has not been inspected or created;
- GitHub Actions CI workflow is not yet present or verified remotely;
- release packaging workflow and local release artifacts are not yet present or
  verified;
- final clean Debug and Release validation for this publication preparation has
  not yet been run;
- existing release-candidate checklist records manual visual viewer validation
  as incomplete.

## Release-Readiness Conclusion

`NO-GO — publication blocked`

Do not publish until the blockers above are remediated, final local validation
passes, and a later checklist records `GO — ready for public publication`.
