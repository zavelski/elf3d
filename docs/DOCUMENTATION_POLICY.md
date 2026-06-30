# Documentation Policy

Purpose: Define how Elf3D documentation is owned, verified, and kept in sync
with implementation changes.

Applicable version: 0.7.2

Document status: Living policy.

Last verified Git commit: local tag `v0.7.2` after release commit

Implementation source paths: `AGENTS.md`, `docs`, `README.md`,
`PROJECT_STATE_EN.md`

Known limitations: No automated documentation checker is currently committed.
This policy relies on task discipline and review until a lightweight checker is
added.

Related documents: `DOCUMENTATION_UPDATE_CHECKLIST.md`, `README.md`,
`../README.md`, `../PROJECT_STATE_EN.md`

## Authoritative Documents

Repository-level working rules:

- `AGENTS.md`
- `CODING_POLICY.md`
- `ARCHITECTURE.md`
- `docs/DOCUMENTATION_POLICY.md`
- `docs/DOCUMENTATION_UPDATE_CHECKLIST.md`

Living technical documents:

- `PROJECT_STATE_EN.md`
- `docs/PUBLIC_API_OVERVIEW.md`
- `docs/MODULE_MAP.md`
- `docs/GLTF_SUPPORT.md`
- `docs/RENDERING_PIPELINE.md`
- `docs/VIEWPORT_AND_TOOLS.md`
- `docs/LIFETIME_AND_THREADING.md`
- `docs/TESTING.md`
- `docs/PERFORMANCE_BASELINE.md`
- `docs/ROADMAP.md`
- `docs/USER_GUIDE.md`

Release snapshots:

- `docs/releases/<version>/...`

Audit records:

- `docs/audits/...`

Audit records preserve what was known at the time. If a later goal changes the
state, update the living documents and add a new audit/remediation entry rather
than rewriting historical facts without explanation.

## Required Review Triggers

Review and update documentation when a task affects:

- public API or exported symbols
- CMake targets, presets, module dependencies, or binary outputs
- third-party dependencies or pinned revisions
- glTF support, limits, warnings, or importer behavior
- render passes, shaders, color-space policy, OpenGL state, or GPU caches
- viewport input, navigation, picking, selection, visibility, isolation,
  measurement, or clipping
- ownership, lifetime, threading, shutdown, or cache invalidation
- tests, fixtures, validation commands, CI, or manual validation procedure
- viewer behavior, user-visible controls, diagnostics, or error messages
- performance measurements or benchmark procedure
- roadmap status, release blockers, known limitations, or release decisions

## Document Ownership by Subsystem

| Subsystem | Primary documents |
| --- | --- |
| Public API | `PUBLIC_API_OVERVIEW.md`, `LIFETIME_AND_THREADING.md` |
| Build and modules | `MODULE_MAP.md`, `TESTING.md` |
| glTF/import | `GLTF_SUPPORT.md`, `USER_GUIDE.md` |
| Rendering/backend | `RENDERING_PIPELINE.md`, `LIFETIME_AND_THREADING.md` |
| Viewport/tools | `VIEWPORT_AND_TOOLS.md`, `USER_GUIDE.md` |
| Testing/validation | `TESTING.md`, audit validation matrix |
| Performance | `PERFORMANCE_BASELINE.md` |
| Release state | `PROJECT_STATE_EN.md`, `ROADMAP.md`, release snapshots |
| Third-party dependencies | `THIRD_PARTY.md`, `MODULE_MAP.md` |

## Status Labels

Use explicit status wording near the top of every technical document:

- `Verified`: checked against current code and validation.
- `Living`: updated as the repository changes.
- `Release snapshot`: immutable record for a specific release.
- `Draft`: incomplete and not authoritative.
- `Not measured`: performance or validation data has no measurement.

Do not use planned or candidate features in implemented-feature lists.

## Version and Verification Fields

Every technical document should include:

- purpose
- applicable version
- document status
- last verified Git commit
- implementation source paths
- known limitations
- related documents

When implementation changes invalidate a document, update the verification
commit and the affected sections.

## Verification Procedure

1. Read the affected source, tests, and nearest documentation.
2. Determine whether code, documentation, or both should change.
3. Update living docs in the same task as implementation when behavior changes.
4. Run relevant validation or record why it could not be run.
5. Check that source paths and referenced documents exist.
6. Do not claim manual viewer or rendering validation unless it was actually
   performed.
7. Preserve release snapshots; create new release notes or living updates
   instead of silently rewriting old snapshots.

## Handling Code/Documentation Mismatches

When code and documentation disagree:

1. Treat code and tests as the implementation source of truth.
2. Determine whether the documented behavior was intended and should be
   implemented.
3. If the documented behavior is speculative or out of scope, correct the docs.
4. If the implementation is wrong, fix code and add or update tests.
5. Record remaining limitations in the relevant document and release notes.
