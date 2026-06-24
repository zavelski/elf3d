---
name: elf3d-publish-change
description: "Validate, document, commit, push, create or update a PR, and verify ordinary Elf3D changes on GitHub. Use when the user asks to create a commit on GitHub, commit and push, publish current changes, upload completed work, synchronize an ordinary change, or says in Russian: Создай коммит на GitHub, Зафиксируй текущие изменения на GitHub, Опубликуй текущие изменения. Do not use for version tags, merges into main, version releases, or GitHub Releases."
---

# Elf3D Publish Change

Use this skill for ordinary Elf3D change publication. Do not use it for version
release work; use `$elf3d-release` for named releases, version tags, GitHub
Releases, or final `main` and `develop` synchronization.

## Required References

Read these before acting:

- `references/branch-policy.md` before choosing or changing branches.
- `references/validation-policy.md` before choosing validation.
- Root `AGENTS.md`, `CODING_POLICY.md`, `ARCHITECTURE.md`, and affected living
  documentation before editing, staging, or committing.

## Workflow

1. Inspect the repository root, current branch, tracking branch, staged changes,
   unstaged changes, untracked files, ignored files where relevant, current
   local commit, `origin`, remote branch state, local and remote tags, and
   GitHub authentication when GitHub operations are required.
2. Run `git fetch origin --prune --tags` before deciding how to synchronize.
3. Verify that `origin` is `zavelski/elf3d`. Stop if it is not.
4. Review every changed file. Do not assume `git status` alone is sufficient.
5. Scan for unsafe or private material before staging:
   API keys, tokens, passwords, private keys, `.env` files, private URLs,
   customer models, CET or Revit customer exports, Yulio data, private LWNative
   code, build outputs, packages, logs, dumps, `imgui.ini`, IDE state,
   user-specific absolute paths, and unexpectedly large binaries.
6. Classify the change using `references/validation-policy.md`.
7. Update affected living documentation in the same change when behavior,
   validation, workflow, CI, release status, or public-facing guidance changes.
8. Run appropriate validation. Prefer the helper:

   ```powershell
   .\.agents\skills\elf3d-publish-change\scripts\validate-change.ps1 -Mode Docs
   .\.agents\skills\elf3d-publish-change\scripts\validate-change.ps1 -Mode Code
   .\.agents\skills\elf3d-publish-change\scripts\validate-change.ps1 -Mode Full
   .\.agents\skills\elf3d-publish-change\scripts\validate-change.ps1 -Mode Package
   ```

9. Stage files explicitly. Never use `git add .`.
10. Create one or more logical Conventional Commit commits. Use only:
    `feat:`, `fix:`, `refactor:`, `perf:`, `test:`, `docs:`, `build:`,
    `ci:`, or `chore:`.
11. Fetch again before pushing. Confirm the destination branch and ahead/behind
    state.
12. Push only the intended branch. Never use `--force`, `--force-with-lease`,
    `--all`, or `--tags`.
13. Create or update a pull request targeting `develop` when appropriate. Do
    not merge automatically unless the user explicitly authorizes it.
14. Monitor CI for the exact pushed commit. Diagnose failing logs, correct
    repository-caused failures with a new commit, validate again, push normally,
    and recheck CI. Stop on authentication, billing, runner, or service
    blockers.

## Branch Behavior

- On `main`, never commit ordinary development directly. Preserve all work,
  create an appropriate task branch, and continue there.
- On `develop`, small bounded maintenance or documentation changes may commit
  directly. Significant or risky work should use a task branch and PR.
- On a task branch, commit and push that branch. Target `develop` for the PR
  unless the user gives a specific, safe reason otherwise.

## Completion Report

Report the starting branch and commit, resulting branch, created commits and
messages, pushed branch, PR URL when available, local validation commands and
actual test totals, CI result, documentation updates, remaining manual checks,
and final working-tree status.

Explicitly state anything not verified. Do not claim viewer, CI, PR, or GitHub
verification that was blocked by missing tooling or authentication.
