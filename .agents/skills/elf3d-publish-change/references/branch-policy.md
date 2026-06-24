# Elf3D Ordinary Publication Branch Policy

Read this reference before deciding where an ordinary change should be
committed or pushed.

## Branch Model

`main` is the latest stable public state. It contains the most recently
released source baseline and intentional post-release documentation commits. It
must not receive incomplete ordinary development.

`develop` is the integration branch for completed development work. It may be
ahead of `main` during normal development.

At the end of a completed release workflow, `origin/main` and `origin/develop`
must point to the same final post-release commit. Ordinary publication must not
perform that release synchronization.

Use task branches for significant work:

```text
feature/<clear-name>
fix/<clear-name>
docs/<clear-name>
chore/<clear-name>
ci/<clear-name>
```

Feature and fix branches normally target `develop`.

## Ordinary Work on `main`

Never commit ordinary development directly to `main`. If valid uncommitted
ordinary work exists on `main`, preserve it, create an appropriate task branch,
and continue there. Do not reset, hide, discard, or overwrite user changes.

## Ordinary Work on `develop`

Small bounded maintenance or documentation changes may be committed directly to
`develop` when the change is low risk and coherent. Significant, risky,
multi-commit, public API, CMake, dependency, rendering, packaging, or CI work
should use a task branch and a pull request into `develop`.

## Ordinary Work on a Task Branch

Commit and push the same task branch. Create or update a pull request targeting
`develop` when appropriate. Do not merge the pull request unless the user
explicitly authorizes automatic merge for that request.

## Release Boundaries

Ordinary publication must never:

- merge ordinary work into `main`;
- create, delete, or move a version tag;
- create a GitHub Release;
- change the project version as a release operation;
- publish formal release packages unless packaging itself is the ordinary
  change;
- synchronize `main` and `develop` as a release finalization step;
- force-push.

Those responsibilities belong to `$elf3d-release`.
