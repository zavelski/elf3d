# Roadmap

Purpose: Track committed, candidate, and exploratory work after the 0.1.0 audit.

Applicable version: 0.1.0

Document status: Living roadmap derived from audit findings and current code.

Last verified implementation commit before release snapshot: `79fd4bc`

Implementation source paths: `docs/audits/ELF3D_0.1.0_AUDIT.md`,
`docs/audits/ELF3D_0.1.0_REMEDIATION_LOG.md`, `README.md`, `modules`,
`apps/viewer`

Known limitations: This roadmap is not a promise of delivery. Candidate and
exploratory items require separate design, implementation, and validation.
Manual viewer validation for the 0.1.0 publication baseline has been completed
by the user on the packaged Windows Release viewer.

Related documents: `../PROJECT_STATE_EN.md`, `GLTF_SUPPORT.md`,
`PERFORMANCE_BASELINE.md`, `TESTING.md`

## Committed for 0.1.0 Baseline

| Item | Motivation | Dependency | Benefit | Risk | Validation |
| --- | --- | --- | --- | --- | --- |
| Static scene and asset API | Embed a minimal visualization engine. | Current public headers. | Host can create scenes, cameras, meshes, materials. | C++ ABI compatibility must be documented. | Public API tests. |
| OpenGL off-screen viewport | First working render path. | Host OpenGL context. | Viewer and hosts can render into a texture. | Context lifetime is host-sensitive. | Renderer/viewport tests and manual viewer validation. |
| Static glTF/GLB importer | Load real model files. | cgltf, image decoder, scene builder. | Supports bounded static triangle assets. | Unsupported glTF features must be clear. | glTF importer tests. |
| Picking, selection, measurement, clipping | Provide interactive visualization tools. | Viewport, scene, renderer filters. | Reference viewer can inspect geometry. | Manual behavior needs visual validation. | Tool tests plus viewer validation. |
| Safe scene cache release context | Remove raw engine pointer hazard. | Goal 4 remediation. | Late scene destruction is a safe no-op instead of raw pointer dereference. | Still not a license to violate host ownership order. | Public lifetime smoke and CTest. |
| Release snapshot and checklist | Make 0.1.0 auditable. | Goals 5-7 docs. | Prevents false release claims. | Tag must not be created before manual viewer validation. | Release checklist review. |

## Release Completion Gates

| Item | Motivation | Dependency | Expected benefit | Risk | Validation |
| --- | --- | --- | --- | --- | --- |
| Manual viewer validation | Verify the actual user-facing rendering/tool path. | Packaged Windows Release viewer. | Confirms graphics and interaction behavior. | Manual validation is not automated. | Completed by the user for the 0.1.0 publication baseline. |
| Final release decision | Decide readiness honestly. | Validation and documentation. | Blocks premature `v0.1.0`. | None if required validation remains respected. | `GO — ready for public publication`. |
| Publication verification | Verify the public repository, CI, release assets, and clone path. | GitHub publication steps. | Confirms the published artifact is usable. | Publication must stop if CI or clone validation fails. | Completed for `v0.1.0`: branch CI, tag-triggered GitHub Release, asset download/checksum verification, and public clone test passed. |

## 0.1.x Candidate Corrections

| Item | Motivation | Dependency | Expected benefit | Risk | Validation |
| --- | --- | --- | --- | --- | --- |
| Public import warning reporting | `std::clog` warnings are not host-controllable. | Public API design. | GUI hosts can display import warnings. | API addition must preserve source compatibility where practical. | Facade tests and viewer warning display. |
| Documentation path checks | Keep docs synchronized. | Documentation policy. | Reduces stale source references. | Avoid adding heavy tooling. | Lightweight script or CTest. |
| Debug diagnostics for skipped GL deletes | Make shutdown mistakes visible. | Backend policy decision. | Easier host integration debugging. | Low-level logging policy must stay clean. | Backend tests where possible plus manual shutdown. |
| Public header self-containment test | Verify include quality. | Build script/test addition. | Catches missing includes. | More test target maintenance. | CTest target. |

## Proposed 0.2.0 Scope Candidates

| Item | Motivation | Dependency | Expected benefit | Risk | Validation |
| --- | --- | --- | --- | --- | --- |
| Broader glTF materials | Improve model fidelity. | Renderer material model. | Normal, occlusion, emissive support. | Larger shader/material complexity. | Fixture corpus and visual tests. |
| Alpha mask/blend rendering | Support more glTF assets. | Render sorting/blend policy. | Transparent and cutout assets. | Sorting and depth interactions. | Visual regression cases. |
| Scene-wide acceleration | Improve picking scale. | Scene revision invalidation. | Faster large-scene picking. | Cache invalidation complexity. | Picking benchmarks and tests. |
| Viewer file UX improvements | Better reference app workflow. | Current viewer. | Easier validation. | Keep viewer from becoming a full editor. | Manual viewer checklist. |
| Performance baseline tooling | Ground optimization work. | Instrumentation procedure. | Repeatable metrics. | Bad metrics can mislead. | Benchmark docs and scripts. |

## Later Exploratory Work

- Runtime plugin ABI.
- Additional graphics backends.
- Animation, skins, and morph targets.
- Transform gizmos and scene editing.
- Multi-selection and annotations.
- Background loading or render threading.
- CI with graphics-capable smoke tests.

## Explicitly Deferred or Rejected for 0.1.0

- ECS.
- Global event bus.
- Service locator.
- Multiple graphics backends before the OpenGL backend is fully validated.
- Full editor framework.
- Custom memory manager.
- Universal plugin framework.
