# Elf3D Release Checklist

Use this checklist for every named release. A release must have an explicit
`MAJOR.MINOR.PATCH` version.

## Phase 1 - Preflight

- Verify the working tree is clean.
- Verify `origin` is `zavelski/elf3d`.
- Run `git fetch origin --prune --tags`.
- Verify no conflicting remote tag or GitHub Release exists.
- Verify the release version is explicit and semver-shaped.
- Verify no unsafe or private content is present.
- Verify `LICENSE`, `THIRD_PARTY.md`, and third-party notices are current.
- Verify intended release work is integrated into `develop`.
- Verify no unintended open task branch is required for the release.
- Verify GitHub authentication and runner availability before publication
  operations.

## Phase 2 - Prepare on `develop`

- Update all version declarations.
- Update `CHANGELOG.md`.
- Update living docs that describe current behavior, validation, support, or
  release state.
- Create or update `docs/releases/<version>/`.
- Prepare GitHub release notes.
- Complete release readiness records.
- Commit release preparation to `develop`.
- Push `develop` and require green CI before continuing.

## Phase 3 - Local Validation

Run fresh:

```powershell
cmake --fresh --preset windows-debug
cmake --build --preset windows-debug --parallel
ctest --preset windows-debug --output-on-failure
cmake --fresh --preset windows-release
cmake --build --preset windows-release --parallel
ctest --preset windows-release --output-on-failure
.\scripts\package_release.ps1 -Version <version>
```

Also perform viewer smoke validation, required manual validation, package
archive inspection, extracted-package smoke launch, and SHA-256 verification.

Select exactly one outcome:

```text
GO - ready for public release
```

or:

```text
NO-GO - publication blocked
```

Stop on `NO-GO`.

## Phase 4 - Publish Source

- Fetch remote state again.
- Verify no unexpected divergence.
- Merge or fast-forward validated `develop` into `main`.
- Push `main`.
- Require green CI on `main`.
- Record the full SHA of the release source commit now at `main`.

## Phase 5 - Tag

- Verify the remote tag does not already exist.
- Create an annotated `vMAJOR.MINOR.PATCH` tag on the validated release source
  commit.
- Push only the intended tag.
- Verify the remote tag object and peeled target.
- Monitor the tag-triggered release workflow.
- Never move a published tag.

## Phase 6 - GitHub Release

- Create the GitHub Release from the published tag.
- Use verified release notes.
- Upload only verified assets plus `SHA256SUMS.txt`.
- Do not publish an SDK archive unless it was validated.
- Verify filenames, sizes, downloads, checksums, visibility, and tag
  association.
- Perform a fresh public clone test from the tag.

## Phase 7 - Publication Report

After the release is public, create
`docs/releases/<version>/PUBLICATION_REPORT.md` with publication date, release
URL, CI run identifiers, release workflow result, uploaded assets, checksums,
public clone-test result, tag object and target, and known limitations.

Do not move the tag to include this report.

## Phase 8 - Final Synchronization

Use `branch-synchronization.md`. Completion requires:

```text
git rev-parse origin/main
git rev-parse origin/develop
```

to return identical commit IDs.

## Phase 9 - Remote Verification

- Verify `origin/main == origin/develop`.
- Verify there are no release-related commits unique to either branch.
- Verify default branch remains `main`.
- Verify the tag target is unchanged.
- Verify the GitHub Release remains attached to the tag.
- Verify CI is green for the final branch state.
