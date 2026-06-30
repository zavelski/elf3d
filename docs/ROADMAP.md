# Roadmap

Purpose: Track completed 0.7.2 work and candidate or exploratory follow-up work.

Applicable version: 0.7.2

Document status: Living roadmap derived from audit findings and current code.

Last verified Git commit: local tag `v0.7.2` after release commit

Implementation source paths: `docs/audits/ELF3D_0.1.0_AUDIT.md`,
`docs/audits/ELF3D_0.1.0_REMEDIATION_LOG.md`, `README.md`, `modules`,
`apps/viewer`

Known limitations: This roadmap is not a promise of delivery. Candidate and
exploratory items require separate design, implementation, and validation.
Manual viewer validation for the 0.1.0 publication baseline was completed by
the user on the packaged Windows Release viewer. 0.2.0 release validation is
tracked under `docs/releases/0.2.0/`. Published 0.7.1 and 0.7.2 verification is
recorded under their release directories.

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
| Manual viewer validation | Verify the actual user-facing rendering/tool path. | Packaged Windows Release viewer. | Confirms graphics and interaction behavior. | Manual validation is not automated. | Required before public publication. |
| Final release decision | Decide readiness honestly. | Validation and documentation. | Blocks premature version tags. | None if required validation remains respected. | Record `GO - ready for public release` only after every gate passes. |
| Publication verification | Verify the public repository, CI, release assets, and clone path. | GitHub publication steps. | Confirms the published artifact is usable. | Publication must stop if CI or clone validation fails. | Completed for `v0.1.0`, `v0.7.1`, and `v0.7.2`: branch CI, tag-triggered GitHub Release, asset download/checksum verification, and public clone test passed. |

## 0.1.x Candidate Corrections

| Item | Motivation | Dependency | Expected benefit | Risk | Validation |
| --- | --- | --- | --- | --- | --- |
| Documentation path checks | Keep docs synchronized. | Documentation policy. | Reduces stale source references. | Avoid adding heavy tooling. | Lightweight script or CTest. |
| Debug diagnostics for skipped GL deletes | Make shutdown mistakes visible. | Backend policy decision. | Easier host integration debugging. | Low-level logging policy must stay clean. | Backend tests where possible plus manual shutdown. |
| Public header self-containment test | Verify include quality. | Build script/test addition. | Catches missing includes. | More test target maintenance. | CTest target. |

## Completed For 0.7.2

| Item | Motivation | Dependency | Expected benefit | Risk | Validation |
| --- | --- | --- | --- | --- | --- |
| Blended alpha-channel equation | Keep viewport texture alpha physically consistent with source-over composition. | OpenGL blend state. | Opaque clears remain opaque after transparent draws; host compositing gets correct alpha. | Broader reference-image coverage remains pending. | `elf3d.opengl_render_smoke` alpha-pixel check. |
| Display-resolve resource cleanup | Avoid retaining resolve resources after a target is explicitly resized to zero. | OpenGL render-target lifetime. | Reduces GPU resource lifetime tail for zero-size viewports. | Requires current context, as before. | Debug/Release build and OpenGL smoke. |
| Release workflow version derivation | Remove duplicated version literals from release automation. | CMake project version and tag naming. | Reduces tag/package/release-note drift. | Workflow still depends on GitHub runner/toolchain availability. | Local script smoke and tag workflow validation. |
| Deterministic package ZIP metadata | Stabilize archive output for identical staged files. | Package script. | Entry order/timestamps no longer explain local/CI hash drift. | MSVC binary reproducibility is not claimed. | Two local package runs produced identical ZIP hashes. |
| 0.7.2 release preparation | Establish a coherent source/package baseline. | Version metadata, release workflow, package script, docs. | 0.7.2 can be published without manual version edits in the workflow. | Manual viewer validation remains limited. | Debug/Release validation and release records. |

## Completed For 0.7.1

| Item | Motivation | Dependency | Expected benefit | Risk | Validation |
| --- | --- | --- | --- | --- | --- |
| About dialog first-open centering | Remove first-use viewer polish regression. | Dear ImGui next-window positioning. | About dialog opens centered on the first and later invocations across window sizes. | Manual GUI verification is still required. | Viewer build; manual checklist item. |
| Hover-wheel 3D view dolly | Make wheel zoom respond when the cursor is inside the 3D view without requiring a click to restore focus. | Viewer ImGui hover routing and orbit navigation input handling. | More predictable docked viewport navigation. | Must not steal wheel from active UI/popup paths. | Navigation regression test and viewer build. |
| Stable wheel after quick click | Prevent a click-derived off-axis pivot from causing later wheel zoom to rotate or jump the camera. | Orbit navigation pivot alignment. | Wheel zoom remains smooth after quick click, mouse movement, and focus changes. | Dynamic pivot behavior must remain available for intentional examine/orbit use. | Navigation regression test. |
| Linear transparency composition | Blend transparent materials before display transfer encoding. | OpenGL backend display resolve. | Avoids output-space alpha blending in the viewport target. | Display resolve adds one pass and broader golden-image coverage is still pending. | `elf3d.opengl_render_smoke` pixel test. |
| Real OpenGL smoke test | Exercise shader compilation and framebuffer readback in automation. | Hidden GLFW OpenGL context. | Catches GLSL/state/pixel regressions that fake-device tests cannot. | Test skips on machines without OpenGL 4.1 context. | CTest with skip return code 77. |
| Expanded glTF index limit | Bound imported triangle-list memory after strip/fan conversion. | Importer resource-limit validation. | Prevents strip/fan expansion from bypassing index memory limits. | None for supported primitive modes. | glTF importer oversized strip fixture. |
| Host-owned load diagnostics | Keep scene-load warnings out of global streams. | Existing `load_scene_with_report()` API. | Hosts choose how diagnostics are displayed. | Callers of compatibility `load_scene()` do not receive warnings. | Public API and viewer documentation. |
| Published 0.7.1 release | Verify the release path after local preparation. | GitHub workflows and release assets. | Confirms branch CI, tag workflow, assets, and public clone. | Manual viewer validation remained limited. | `docs/releases/0.7.1/PUBLICATION_REPORT.md`. |

## Completed For 0.6.0

| Item | Motivation | Dependency | Expected benefit | Risk | Validation |
| --- | --- | --- | --- | --- | --- |
| UV1 and texture transforms | Remove a practical production-file blocker. | Bounded vertex/material mappings. | `TEXCOORD_1`, per-slot `texCoord`, and `KHR_texture_transform` render correctly. | Fixed two-set limit must be explicit. | Importer, renderer, public API, and viewer tests/builds. |
| Broader static materials | Improve model fidelity. | Renderer material model. | Alpha, vertex color, emissive, occlusion, unlit, IOR, and specular factors. | Normal mapping still requires a tangent-space program. | Generated fixtures, shader-contract tests, viewer validation. |
| Structured import diagnostics | Make fallbacks host-visible. | Additive public API. | GUI hosts can display warnings without relying on `std::clog`. | Public C++ ABI requires rebuild. | Public API test and viewer Model Information panel. |
| Primitive/camera compatibility | Load more valid static scenes. | Existing triangle renderer and scene camera model. | Strip/fan conversion and perspective-camera import. | Orthographic/lights remain fallback-only. | Importer fixtures. |
| Private corpus probe | Validate uncommitted real files repeatably. | Public load-report API. | Per-file errors, diagnostics, statistics, and timing without publishing models. | Results depend on locally supplied corpus. | Project fixture probe plus optional CTest registration. |

## Proposed Next glTF Compatibility Scope

| Item | Motivation | Dependency | Expected benefit | Risk | Validation |
| --- | --- | --- | --- | --- | --- |
| Tangent-space normal mapping | Complete the most visible remaining core material gap. | Tangent import/generation and shader basis. | Correct normal textures on production assets. | Mirrored UVs and handedness require rigorous tests. | Khronos feature fixtures plus real corpus and pixel checks. |
| KTX2/BasisU and compression decision | Many production assets use compressed geometry/textures. | Approved pinned decoder dependencies. | More files load without fallback assets. | Dependency, licensing, memory, and security surface. | Licensed corpus, malformed inputs, memory limits. |
| Scene punctual lights | Preserve authored lighting. | Scene light model and renderer integration. | `KHR_lights_punctual` fidelity. | Must not force light policy into Viewport or break scene boundaries. | Scene/renderer/importer tests and visual cases. |
| Transparent render validation | Raise confidence in mask/blend behavior. | Real OpenGL test path. | Catch shader/state/order regressions. | Golden-image stability. | Reference pixels/images and viewer corpus. |
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
