# Documentation Update Checklist

Purpose: Provide a repeatable checklist for human developers and Codex when a
task may affect Elf3D documentation.

Applicable version: 0.1.0

Document status: Living checklist.

Last verified Git commit: `145bbdd`

Implementation source paths: `docs/DOCUMENTATION_POLICY.md`, `AGENTS.md`,
`docs`, `README.md`

Known limitations: This is a manual checklist. No automated checker is committed
yet.

Related documents: `DOCUMENTATION_POLICY.md`, `README.md`, `../README.md`

## Before Editing

- Read `AGENTS.md`, `CODING_POLICY.md`, `ARCHITECTURE.md`, and the nearest
  relevant docs.
- Identify whether the task changes public API, modules, glTF, rendering,
  viewport/tools, lifetime/threading, tests, viewer behavior, performance, or
  roadmap status.
- Decide whether code, docs, or both should change.
- Identify the documents that own the affected subsystem.

## During Editing

- Keep implementation and documentation changes in the same task when behavior
  changes.
- Do not document planned work as implemented.
- Use actual class, function, target, file, and test names.
- Keep release snapshots immutable.
- Mark unmeasured performance values as `Not measured`.
- Mark unverified viewer/manual behavior as not verified.
- Update known limitations when support is partial or intentionally absent.

## Required Document Checks

For every touched technical document, verify:

- purpose is present
- applicable version is present
- document status is present
- last verified Git commit is present
- implementation source paths are present
- known limitations are present
- related documents are present
- referenced source paths exist or are clearly glob patterns
- validation claims match commands actually run
- manual viewer claims match actual manual validation

## Common Trigger Map

| Change type | Documents to inspect |
| --- | --- |
| Public API or exported symbol | `PUBLIC_API_OVERVIEW.md`, `LIFETIME_AND_THREADING.md`, `PROJECT_STATE_EN.md` |
| CMake target/dependency | `MODULE_MAP.md`, `TESTING.md`, `PROJECT_STATE_EN.md` |
| glTF importer | `GLTF_SUPPORT.md`, `USER_GUIDE.md`, `ROADMAP.md` |
| Renderer/backend/shader | `RENDERING_PIPELINE.md`, `LIFETIME_AND_THREADING.md`, `USER_GUIDE.md` |
| Viewport/input/navigation | `VIEWPORT_AND_TOOLS.md`, `USER_GUIDE.md` |
| Picking/selection/visibility/isolation | `VIEWPORT_AND_TOOLS.md`, `USER_GUIDE.md`, `TESTING.md` |
| Measurement/clipping | `VIEWPORT_AND_TOOLS.md`, `USER_GUIDE.md`, `TESTING.md` |
| Lifetime/threading/shutdown | `LIFETIME_AND_THREADING.md`, `PUBLIC_API_OVERVIEW.md`, `README.md` |
| Tests/fixtures/validation | `TESTING.md`, audit validation matrix, `PROJECT_STATE_EN.md` |
| Performance | `PERFORMANCE_BASELINE.md`, `ROADMAP.md` |
| Release readiness | `PROJECT_STATE_EN.md`, `ROADMAP.md`, release snapshot |

## Before Commit

- Run relevant build/tests or record exactly why validation was skipped.
- Run `git diff --check`.
- Review the full diff for unrelated changes.
- Confirm ignored build outputs are not staged.
- Ensure documentation updates do not contradict `ARCHITECTURE.md` or
  `CODING_POLICY.md`.
- Summarize what was validated and what remains unverified.
