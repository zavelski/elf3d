---
name: elf3d-release
description: "Prepare, validate, publish, verify, and finalize an Elf3D named version release. Use only when the user explicitly asks to release a version, publish a named version, merge a validated version into main, create a version tag, publish release packages, create a GitHub Release, or says in Russian: Выпусти Elf3D 0.2.0. Requires an explicit version and must finish with remote main and develop synchronized to the same final commit."
---

# Elf3D Release

Use this skill only for explicit named releases, such as:

```text
$elf3d-release Выпусти Elf3D 0.2.0
$elf3d-release Publish version 0.1.1
```

Never infer or increment the version automatically. Stop until the user provides
an explicit `MAJOR.MINOR.PATCH` version.

## Required References

Read these before acting:

- `references/release-checklist.md` for release phases and gates.
- `references/branch-synchronization.md` before touching `main`, `develop`, or
  release tags.
- Root `AGENTS.md`, `CODING_POLICY.md`, `ARCHITECTURE.md`,
  `docs/DOCUMENTATION_POLICY.md`, `docs/DOCUMENTATION_UPDATE_CHECKLIST.md`,
  `docs/TESTING.md`, `.github/workflows/release.yml`, and
  `scripts/package_release.ps1`.

## Release Invariants

A successful release must end with all of these verified remotely:

- `origin/main` and `origin/develop` have identical head commit IDs.
- The `vMAJOR.MINOR.PATCH` annotated tag points to the exact validated release
  source commit.
- The published tag is never moved afterward.
- Post-release documentation, if any, exists in both `main` and `develop`.
- Required CI is green for the release source and final branch state.
- The GitHub Release is attached to the published tag.
- Verified release assets and `SHA256SUMS.txt` are present.
- A fresh public clone test from the tag passed.
- The working tree is clean.

Do not report success until these invariants are true.

## Phases

1. Preflight: require explicit version, inspect local and remote state, verify
   `origin`, fetch branches and tags, reject conflicting tags or releases,
   scan for unsafe content, verify license notices, confirm intended release
   work is integrated into `develop`, and require a clean working tree.
2. Release preparation on `develop`: update version declarations, changelog,
   living docs, `docs/releases/<version>/`, release notes, and checklist.
   Commit, push `develop`, and require green CI before continuing.
3. Local release validation: run fresh Debug and Release configure/build/CTest,
   viewer smoke where possible, required manual validation, release packaging,
   archive inspection, extracted-package smoke launch, and SHA-256 generation.
   Select exactly `GO - ready for public release` or
   `NO-GO - publication blocked`.
4. Merge release source into `main`: fetch again, verify no unexpected
   divergence, merge or fast-forward validated `develop` into `main` according
   to repository policy, push `main`, and require green CI. Record the full
   release source SHA at `main`.
5. Create and publish the annotated tag `vMAJOR.MINOR.PATCH` on the release
   source commit. Push only that tag. Verify the remote tag object and target.
6. Create and verify the GitHub Release from the published tag using verified
   notes, verified assets, and `SHA256SUMS.txt`.
7. Create `docs/releases/<version>/PUBLICATION_REPORT.md` only after the
   release is public. Do not move the tag to include this report.
8. Synchronize final branches as described in
   `references/branch-synchronization.md` until remote `main` and `develop`
   point to the same final commit.
9. Verify remote UI state: identical remote SHAs, no release-related commits
   unique to either branch, default branch still `main`, tag unchanged, release
   attached to the tag, and green CI for the final state.

## Helper Script

Use the helper for repeatable local checks:

```powershell
.\.agents\skills\elf3d-release\scripts\verify-release.ps1 -Version 0.2.0 -TagState Absent
.\.agents\skills\elf3d-release\scripts\verify-release.ps1 -Version 0.2.0 -TagState Present -RequireSynchronizedBranches -RequireGitHubRelease
```

The script supports local verification only. It does not replace manual release
judgment, GitHub CI inspection, GitHub Release asset verification, or public
clone testing.

## Stop Conditions

Stop and report `NO-GO - publication blocked` on failed build, failed test,
incomplete mandatory manual validation, Critical or High release blocker,
unsafe or confidential file, missing license notice, conflicting tag, existing
conflicting release, unexpected remote history, failed CI, invalid package,
invalid checksum, authentication failure, GitHub billing lock, runner outage, or
service error.

## Final Report

Report released version, release source commit, final synchronized branch
commit, remote `main` SHA, remote `develop` SHA, proof they are identical, tag
object and target, GitHub Release URL, release assets, SHA-256 values, CI
status, release workflow status, clone-test result, publication-report commit,
working-tree status, and known limitations.
