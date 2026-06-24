# Codex Skills

Purpose: Document the project-scoped Codex skills used to publish ordinary
Elf3D changes and version releases.

Applicable version: 0.2.0

Document status: Living workflow guide.

Last verified Git commit: pending 0.2.0 release source commit

Implementation source paths: `.agents/skills/elf3d-publish-change`,
`.agents/skills/elf3d-release`, `AGENTS.md`, `.github/workflows`,
`scripts/package_release.ps1`

Known limitations: Skill discovery is performed by Codex from project skill
metadata. GitHub operations still require a working authenticated GitHub path in
the local environment.

Related documents: `../AGENTS.md`, `DOCUMENTATION_POLICY.md`,
`DOCUMENTATION_UPDATE_CHECKLIST.md`, `TESTING.md`

## Skills

`$elf3d-publish-change` validates, documents, commits, pushes, opens or updates
a pull request, and verifies CI for ordinary completed changes. It must not
create version tags, GitHub Releases, or release branch synchronization.

`$elf3d-release` prepares, validates, publishes, verifies, and finalizes a named
Elf3D version release. It requires an explicit version and must end with remote
`main` and `develop` pointing to the same final commit.

## Trigger Examples

Explicit ordinary publication:

```text
$elf3d-publish-change
```

Implicit ordinary publication:

```text
Create a commit on GitHub for the current changes.
```

```text
Создай коммит на GitHub для текущих изменений.
```

Explicit release:

```text
$elf3d-release Выпусти Elf3D 0.2.0
```

Implicit release:

```text
Release Elf3D 0.2.0.
```

The release skill must not run without an explicit version.

## Branch Behavior

`main` is the stable public branch. Ordinary development must not be committed
directly to `main`.

`develop` is the integration branch for completed development. Small bounded
maintenance or documentation changes may land directly on `develop`; significant
changes should use a task branch and pull request targeting `develop`.

Task branches use:

```text
feature/<clear-name>
fix/<clear-name>
docs/<clear-name>
chore/<clear-name>
ci/<clear-name>
```

## Validation Behavior

Ordinary documentation-only changes use repository consistency, whitespace,
Markdown referenced-path, YAML, and PowerShell syntax checks where available.

C++ implementation, shaders, rendering, Scene, Assets, Viewport, Tools, CMake,
dependency, CI, packaging, public API, and performance changes require broader
validation selected by `$elf3d-publish-change`.

Release validation is stricter. `$elf3d-release` runs fresh Debug and Release
configure/build/CTest, package creation and inspection, checksum verification,
viewer smoke/manual validation where applicable, CI verification, GitHub Release
verification, and public clone testing.

## Safety Rules

Both skills must stop before staging or publishing unsafe content, including
secrets, credentials, `.env` files, private URLs, customer models, CET or Revit
exports, Yulio data, private LWNative code, build outputs, packages, logs,
dumps, `imgui.ini`, IDE state, user-specific absolute paths, and unexpectedly
large binaries.

Neither skill may force-push. Neither skill may move a published tag.

## Ordinary Publish vs Release

Ordinary publish means making a completed non-release change safely available on
GitHub through validation, documentation review, logical commits, push, pull
request handling when appropriate, and CI verification.

Release means publishing a named project version with version updates, release
records, package assets, checksums, an annotated version tag, a GitHub Release,
public clone verification, publication report, and final branch
synchronization.

## Final Release Synchronization

Every successful release must finish with:

```text
origin/main == origin/develop
```

The remote head commit IDs must be identical. It is not enough for one branch to
contain the other.

The release tag may point behind the final branch heads because it must remain
attached to the validated release source commit. A later post-release
publication report can advance both branches. The tag must not be moved to
include that report.

## Updating Skills

Update these skills when CI, CMake presets, release packaging, branch policy,
documentation policy, validation commands, tag policy, or GitHub publication
requirements change.

Keep procedural detail in the skill references and helper scripts. Keep
`AGENTS.md` concise so routing remains easy to scan.
