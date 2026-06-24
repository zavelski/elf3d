# Elf3D Release Branch Synchronization

Release completion requires remote `main` and `develop` to point to the same
final commit. Containment is not enough; the head SHA values must be identical.

## Tag Position

The version tag points to the validated release source commit, not necessarily
the final branch head. Post-release documentation may advance both branches, but
the published tag must remain fixed.

Conceptual graph:

```text
A---B---R---P  main
         \   /
          tag vX.Y.Z points to R

A---B---R---P  develop
```

`R` is the validated release source commit. `P` is the optional post-release
publication-report commit. If no post-release commit is needed, both branches
end at `R`.

## Controlled Procedure

1. Create the post-release report commit on `develop`, or on a dedicated
   post-release documentation branch targeting `develop`.
2. Push it and verify CI if applicable.
3. Merge or fast-forward the same post-release documentation commit into
   `main`.
4. Push `main`.
5. Fetch remote state.
6. If `main` now contains a commit not present in `develop`, merge or
   fast-forward `main` back into `develop`.
7. Push `develop`.
8. Repeat until `origin/main` and `origin/develop` have exactly the same SHA.

Final check:

```powershell
git fetch origin --prune --tags
$main = git rev-parse origin/main
$develop = git rev-parse origin/develop
if ($main -ne $develop) { throw "main and develop are not synchronized" }
```

## Prohibitions

- Do not force-push.
- Do not move or recreate a published version tag.
- Do not use branch synchronization to sneak in ordinary unreleased
  development.
- Do not report success while `origin/main` and `origin/develop` differ.

If GitHub continues to show a temporary UI banner after the remote SHA values
are identical, report it as a GitHub UI cache condition rather than repository
divergence.
