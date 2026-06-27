# Roadmap

Purpose: Track completed 0.4.0 work and candidate or exploratory follow-up work.

Applicable version: 0.4.0

Document status: Living roadmap derived from audit findings and current code.

Last verified implementation commit before release snapshot: pending 0.4.0 release source commit

Implementation source paths: `docs/audits/ELF3D_0.1.0_AUDIT.md`,
`docs/audits/ELF3D_0.1.0_REMEDIATION_LOG.md`, `README.md`, `modules`,
`apps/viewer`

Known limitations: This roadmap is not a promise of delivery. Candidate and
exploratory items require separate design, implementation, and validation.
Manual viewer validation for the 0.1.0 publication baseline was completed by
the user on the packaged Windows Release viewer. 0.2.0 release validation is
tracked under `docs/releases/0.2.0/`. Local 0.4.0 release validation is tracked
under `docs/releases/0.4.0/`; publication remains pending manual and remote
release gates.

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

## Committed for 0.2.0

| Item | Motivation | Dependency | Benefit | Risk | Validation |
| --- | --- | --- | --- | --- | --- |
| GPU-first picking | Reduce full CPU traversal for ordinary surface picks. | Renderer picking framebuffer and CPU refinement. | Viewer selection can use the visible render pass candidate before CPU fallback. | Readback and refinement must stay consistent with visibility and clipping. | Picking, renderer, viewport tests plus viewer smoke. |
| Low.3D-inspired viewer refresh | Improve reference viewer usability and visual parity with current Low.3D. | Dear ImGui integration and viewer assets. | Droid Sans font, light panels, generated toolbar PNG icons, compact status strip, right-side dock layout. | Viewer assets must be packaged and licensed. | Debug/Release build, package inspection, viewer screenshot smoke. |
| Repository-local publish/release skills | Make GitHub publication and release gates repeatable. | `.agents/skills`, GitHub CLI, existing CMake presets. | Safer ordinary publishing and named releases. | Tooling is procedural and depends on local auth. | Skill docs and release workflow validation. |

## Release Completion Gates

| Item | Motivation | Dependency | Expected benefit | Risk | Validation |
| --- | --- | --- | --- | --- | --- |
| Manual viewer validation | Verify the actual user-facing rendering/tool path. | Packaged Windows Release viewer. | Confirms graphics and interaction behavior. | Manual validation is not automated. | Required for 0.4.0 before publication. |
| Final release decision | Decide readiness honestly. | Validation and documentation. | Blocks premature version tags. | None if required validation remains respected. | Record `GO - ready for public release` only after every gate passes. |
| Publication verification | Verify the public repository, CI, release assets, and clone path. | GitHub publication steps. | Confirms the published artifact is usable. | Publication must stop if CI or clone validation fails. | Completed for `v0.1.0`: branch CI, tag-triggered GitHub Release, asset download/checksum verification, and public clone test passed. |

## 0.1.x Candidate Corrections

| Item | Motivation | Dependency | Expected benefit | Risk | Validation |
| --- | --- | --- | --- | --- | --- |
| Public import warning reporting | `std::clog` warnings are not host-controllable. | Public API design. | GUI hosts can display import warnings. | API addition must preserve source compatibility where practical. | Facade tests and viewer warning display. |
| Documentation path checks | Keep docs synchronized. | Documentation policy. | Reduces stale source references. | Avoid adding heavy tooling. | Lightweight script or CTest. |
| Debug diagnostics for skipped GL deletes | Make shutdown mistakes visible. | Backend policy decision. | Easier host integration debugging. | Low-level logging policy must stay clean. | Backend tests where possible plus manual shutdown. |
| Public header self-containment test | Verify include quality. | Build script/test addition. | Catches missing includes. | More test target maintenance. | CTest target. |

## Proposed 0.5.x Scope Candidates

| Item | Motivation | Dependency | Expected benefit | Risk | Validation |
| --- | --- | --- | --- | --- | --- |
| Broader glTF materials | Improve model fidelity. | Renderer material model. | Normal, occlusion, emissive support. | Larger shader/material complexity. | Fixture corpus and visual tests. |
| Alpha mask/blend rendering | Support more glTF assets. | Render sorting/blend policy. | Transparent and cutout assets. | Sorting and depth interactions. | Visual regression cases. |
| Scene-wide acceleration | Improve picking scale. | Scene revision invalidation. | Faster large-scene picking. | Cache invalidation complexity. | Picking benchmarks and tests. |
| Viewer file UX improvements | Better reference app workflow. | Current viewer. | Easier validation. | Keep viewer from becoming a full editor. | Manual viewer checklist. |
| Performance baseline tooling | Ground optimization work. | Instrumentation procedure. | Repeatable metrics. | Bad metrics can mislead. | Benchmark docs and scripts. |

## Completed For 0.4.0

| Item | Motivation | Dependency | Expected benefit | Risk | Validation |
| --- | --- | --- | --- | --- | --- |
| C++20 named modules and OBJECT libraries | Move internal architecture away from static-library boundaries while preserving one public DLL. | Visual Studio 2022 v17.14.35, CMake 3.28+ `FILE_SET CXX_MODULES`. | Clearer logical module boundaries, one final DLL product, and less reliance on internal static libraries. | CMake/Visual Studio module support and CI runner tool versions must be verified. | Debug/Release configure/build/CTest plus module-map review. |
| Direct module imports | Remove the transitional internal include surface after module interfaces own declarations. | Completed named-module interfaces and CMake dependency scanning. | One internal declaration surface with explicit imports. | Public headers must remain untouched. | Shim audit, module import smoke, Debug/Release builds and CTest. |

## Later Exploratory Work

- Runtime plugin ABI.
- Additional graphics backends.
- Animation, skins, and morph targets.
- Transform gizmos and scene editing.
- Multi-selection and annotations.
- Background loading or render threading.
- CI with graphics-capable smoke tests.

## Explicitly Deferred or Rejected for Current Scope

- ECS.
- Global event bus.
- Service locator.
- Multiple graphics backends before the OpenGL backend is fully validated.
- Full editor framework.
- Custom memory manager.
- Universal plugin framework.
