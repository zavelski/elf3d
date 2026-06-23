# Elf3D 0.1.0 Technical Audit

Date: 2026-06-23

Repository: `Z:/Elf3D`

Audit branch: `audit/0.1.0`

Checkpoint commit: `f8fe3a827bc81dadb461e58bdbe846958dab346a`

Inventory: `docs/audits/ELF3D_0.1.0_REPOSITORY_INVENTORY.md`

## 1. Executive Summary

Elf3D 0.1.0 is a coherent vertical slice of a portable C++20 3D visualization
engine. The implementation matches the main architectural direction in
`ARCHITECTURE.md`: `elf3d` is the public library, internal modules are static
libraries, OpenGL is isolated in the backend, Dear ImGui and GLFW are confined
to the viewer and optional ImGui integration layer, and public headers do not
expose ImGui, GLFW, GLM, cgltf, GLAD, or OpenGL object types.

The repository is not release-ready yet. The remaining release gates are
documentation and manual validation gates:

- The requested `PROJECT_STATE_EN.md` baseline is absent, so the claimed state
  cannot be compared to the repository.
- The verified release documentation set and release snapshot are incomplete.
- Manual visual viewer validation has not been performed.

The highest-risk implementation finding was lifetime enforcement around
`Scene` destruction. Goal 4 corrected that path by replacing the raw engine
callback context with a private weak release context. Destroying a `Scene` after
its `Engine` is still outside the documented host ownership order, but it no
longer dereferences a freed `Engine::Impl` while releasing renderer and picking
caches.

The rest of the audited slice is generally consistent with the stated 0.1.0
scope: static glTF geometry, opaque metallic-roughness material rendering,
off-screen OpenGL viewport rendering, CPU picking, selection, distance
measurement, scene visibility, viewport isolation, and clipping.

Build and automated test validation passed in Goal 3 after locating Visual
Studio's bundled CMake/CTest executables. Manual visual viewer validation has
not been performed.

Release gate: **blocked**.

## 2. Repository Version Audited

- Active branch before audit: `develop`
- Audit branch: `audit/0.1.0`
- Audit checkpoint commit: `f8fe3a827bc81dadb461e58bdbe846958dab346a`
- Inventory commit: `f0e3750`
- Remotes: none configured
- Tags: none present
- Declared project version: `0.1.0`
- Runtime version data: `modules/core/src/version_data.cpp` returns `0.1.0`
- Public version test: `tests/public_api_test.cpp` expects `0.1.0`

`PROJECT_STATE_EN.md` was requested as a baseline artifact, but no file with
that name exists anywhere under `Z:/Elf3D`.

## 3. Methodology

The initial audit used static inspection. Goal 3 then performed configure,
build, automated test, and non-interactive viewer startup validation with the
Visual Studio bundled CMake and CTest executables.

Reviewed sources:

- Repository guidance: `AGENTS.md`, `CODING_POLICY.md`, `ARCHITECTURE.md`
- User-facing documentation: `README.md`, `THIRD_PARTY.md`
- Build configuration: root `CMakeLists.txt`, `CMakePresets.json`, module
  `CMakeLists.txt` files, `cmake/*.cmake`
- Public API headers under `include/elf3d`
- Facade implementation under `facade/elf3d`
- Internal modules under `modules`
- Viewer and ImGui integration under `apps`
- Unit and integration test definitions under `tests` and module `tests`

Static checks performed:

- Searched for dependency-boundary violations involving ImGui, GLFW, GLM,
  cgltf, GLAD, and OpenGL symbols.
- Reviewed CMake target links for dependency direction.
- Reviewed public headers for third-party type leakage and native graphics
  type leakage.
- Reviewed public facade exception boundaries and ownership comments.
- Reviewed scene, asset, importer, renderer, viewport, picking, selection,
  measurement, clipping, visibility, and navigation implementation paths.
- Reviewed declared tests and their target coverage.
- Attempted the documented configure command.

Validation executed:

```powershell
cmake --fresh --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug --output-on-failure
cmake --fresh --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release --output-on-failure
```

The shell did not have `cmake` or `ctest` on `PATH`, but Visual Studio's bundled
CMake 3.31.6 and CTest 3.31.6 were found and used by absolute path. The full
validation record is in `docs/audits/ELF3D_0.1.0_VALIDATION_MATRIX.md`.

## 4. Confirmed Architecture

The following architectural claims are confirmed by source and build inspection:

- `elf3d` is the only public shared library target.
- Internal functionality is implemented as static-library modules.
- `elf3d_viewer` depends on the public `elf3d` API and `elf3d_imgui`.
- `elf3d_imgui` contains Dear ImGui and GLFW integration.
- Dear ImGui and GLFW do not appear in engine public headers or internal
  engine-core modules.
- OpenGL and GLAD usage is isolated to `elf3d_backend_opengl`, with neutral
  graphics interfaces in `elf3d_graphics`.
- GLM is internal to the math conventions layer and implementation modules; it
  is not exposed from `include/elf3d`.
- cgltf is private to `elf3d_gltf`; `CGLTF_IMPLEMENTATION` appears in exactly
  one private source file.
- stb image is private to `elf3d_image`; `STB_IMAGE_IMPLEMENTATION` appears in
  exactly one private source file.
- Scene code does not link to the renderer.
- Renderer consumes scene and asset data through internal interfaces and
  maintains render-specific GPU caches rather than owning logical scene data.
- Viewport interaction, picking, selection, measurement, clipping, and
  navigation use neutral public value types, not Dear ImGui state.
- Built-in modules are composed explicitly through CMake and the facade; no
  hidden global self-registration pattern was found.

## 5. Subsystem Findings

### Public API

Public headers use namespace `elf3d` and `ELF3D_API` for exported public
classes and functions. The API uses Pimpl ownership for `Engine`, `Scene`,
`Viewport`, and `SceneHierarchySnapshot`. Public value types are project-owned
plain data structures. No ImGui, GLFW, GLM, cgltf, GLAD, or raw OpenGL object
types are exposed.

The API is a C++ DLL API, not a C ABI. It exposes standard library types such
as `std::unique_ptr`, `std::filesystem::path`, `std::optional`, `std::span`,
`std::string_view`, and the template `Result<T>`. That is acceptable for an
initial C++ SDK only if the release documentation states the compiler and CRT
compatibility expectations.

### Engine and Facade

`Engine::create` creates an OpenGL device, renderer, and picking service. Public
methods generally catch exceptions at facade boundaries and translate them to
`Result<T>` errors.

`Engine::load_scene` creates a fresh scene and imports into it, so failed loads
do not modify an existing scene. Import warnings are written to `std::clog`
instead of being returned to the host application.

### Scene and Assets

Scene storage owns logical entities, hierarchy, transforms, cameras, models,
visibility, bounds, and scene-owned assets. Entity and asset handles embed scene
identity. Destroyed entity slots are not reused, so stale entity IDs do not
alias new entities in the current storage model.

Assets validate mesh, image, texture, and material inputs. Meshes are immutable
indexed triangle lists; images are tightly packed RGBA8; material factors are
validated and clamped through project-owned value types.

### glTF Import

The importer implements a bounded static glTF 2.0 subset:

- `.gltf` and `.glb`
- default scene, first-scene fallback, and parentless-node fallback
- node hierarchy, names, TRS and matrix transforms
- triangle-list meshes with indexed and non-indexed primitives
- `POSITION`, optional `NORMAL`, optional `TEXCOORD_0`
- generated normals when enabled and normals are absent
- PBR metallic-roughness factors and base-color/metallic-roughness textures
- PNG/JPEG external images, data URI images, and GLB buffer-view images
- external, GLB, and data URI buffers
- unknown required extensions rejected
- optional extensions not interpreted

The importer deliberately treats `MASK` and `BLEND` materials as opaque and
emits warnings. It also ignores the fourth component of `baseColorFactor` by
setting imported material alpha to `1.0F`.

### Renderer and OpenGL Backend

The renderer builds render lists from scene data and maintains GPU mesh and
texture caches. It renders opaque PBR-style directional lighting into an RGBA8
off-screen target and overlays neutral line/marker primitives for tools.

Native OpenGL work is isolated in `elf3d_backend_opengl`. The backend validates
current-context/thread conditions for normal operations. Destructors skip GL
deletion when the context/thread is unavailable, which avoids invalid GL calls
but can leak GPU resources if the host violates the documented shutdown order.

### Viewport, Navigation, Picking, and Tools

`OffscreenViewport` owns per-viewport state for navigation, selection,
measurement, clipping, visibility isolation, overlays, and render-target
interaction. It consumes scene storage and delegates rendering to the renderer.

Picking is CPU-based. It constructs perspective-camera rays in viewport pixel
coordinates, filters visible renderable entities, tests transformed bounds,
uses per-mesh BVHs, respects material sidedness when requested, and applies
clipping filters to candidate hits.

Selection, distance measurement, clipping, and visibility controllers are
private modules built around neutral public state snapshots. They do not query
Dear ImGui directly.

### Viewer

The viewer owns GLFW, the OpenGL context, Dear ImGui, GUI construction, frame
presentation, file-open UI, drag-and-drop loading, and public API orchestration.
It does not appear to include private engine headers outside the public API and
`elf3d_imgui` integration surface.

### Tests

Declared CTest coverage includes:

- public API and public lifetime smoke test
- math conventions
- assets
- image decoding
- scene
- interaction
- navigation
- picking
- selection
- measurement
- visibility
- clipping
- renderer
- viewport lifetime
- glTF importer

These tests passed in both Debug and Release during Goal 3 validation.

## 6. Discrepancy Register

### AUD-001: Missing Claimed-State Baseline

- Severity: Critical
- Classification: Missing artifact / release blocker
- Affected files: `PROJECT_STATE_EN.md`
- Expected: The audit request references `PROJECT_STATE_EN.md` as the claimed
  repository state to compare against the actual repository.
- Actual: No `PROJECT_STATE_EN.md` file exists under `Z:/Elf3D`.
- Evidence: `rg --files -g 'PROJECT_STATE*'` and a recursive filesystem search
  returned no matches.
- Risk: The audit cannot prove whether the repository matches its intended
  release baseline. Release notes or tags based on an absent baseline would be
  ungrounded.
- Recommended correction: Create a verified `PROJECT_STATE_EN.md` from the
  inventory and audit results, or update the release process to name the real
  baseline artifact.
- Should code change: No
- Should docs change: Yes
- Suggested stage: Goal 4/5 documentation remediation
- Suggested commit: `docs: add verified Elf3D 0.1.0 project state`

### AUD-002: CMake/CTest Were Not on PATH During Initial Audit

- Severity: Informational
- Classification: Resolved validation environment issue
- Affected files: `CMakePresets.json`, all configured targets
- Expected: Configure, build affected targets, run relevant tests, inspect
  warnings, and launch the viewer when required.
- Actual: `cmake` and `ctest` were not on this PowerShell `PATH`, but Visual
  Studio's bundled CMake/CTest executables existed under the VS installation and
  were usable by absolute path.
- Evidence: `where.exe cmake` and `where.exe ctest` failed; the executables were
  found at `C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/`.
  Debug and Release configure/build/CTest then passed using those paths.
- Risk: Developers using a plain shell may see command-not-found failures even
  when Visual Studio CMake is installed.
- Recommended correction: Optionally document how to use Visual Studio's bundled
  CMake or recommend launching a VS Developer PowerShell with CMake on `PATH`.
- Should code change: No
- Should docs change: Optional
- Suggested stage: Goal 5 testing documentation
- Suggested commit: `docs: document Visual Studio bundled CMake usage`

### AUD-003: Scene Cache-Release Callback Had Dangling-Context Failure Mode

- Severity: High
- Classification: Corrected ownership/lifetime risk
- Affected files: `include/elf3d/elf3d.h`, `include/elf3d/scene.h`,
  `facade/elf3d/src/engine.cpp`, `modules/scene/src/scene.cpp`
- Expected: Objects created by Elf3D have explicit ownership and destruction
  rules across the DLL boundary, and invalid destruction order should not turn
  into a use-after-free hazard when a safer local mechanism is practical.
- Previous actual: `Scene::Impl` stored a `ReleaseCallback` and raw `void*` context.
  `Engine::create_scene` passes `Engine::Impl*` as that context. `Scene::Impl`
  invokes the callback from its destructor to release renderer and picking
  caches. If a host destroys the `Engine` before a `Scene`, the documented
  contract has been violated, but the scene still contains a stale pointer.
- Correction: Commit `7957aee` replaces the raw callback context with a private
  `Scene::ReleaseContext` that stores a weak state token. Scene destruction
  releases renderer and picking caches only when the engine release state is
  still alive.
- Evidence: `Engine::create_scene` now passes `impl_->scene_release_context` to
  `Scene::create`; `Scene::Impl::~Impl` calls `release_context->release`, which
  locks a weak state token before invoking the cache-release callback.
- Risk before correction: A host lifetime bug could become a crash or memory corruption during
  scene destruction. The behavior is especially risky because the public API is
  intended to cross a DLL boundary.
- Remaining risk: The documented ownership rule still requires hosts to destroy
  `Scene` and `Viewport` objects before the creating `Engine`. The new public
  lifetime smoke is not sanitizer-backed coverage for every invalid lifetime
  sequence.
- Recommended correction: Completed for 0.1.0. Keep the host ownership rule in
  release documentation.
- Should code change: Completed
- Should docs change: Yes, document the ownership rule and the safer failure mode
- Suggested stage: Goal 4 remediation
- Suggested commit: Completed as `7957aee` (`fix: make scene cache release lifetime-safe`)

### AUD-004: Import Warnings Are Logged Instead of Returned

- Severity: Medium
- Classification: API/reporting gap
- Affected files: `facade/elf3d/src/engine.cpp`, `include/elf3d/elf3d.h`,
  `include/elf3d/scene.h`, `README.md`
- Expected: Expected failures and host-visible diagnostics should flow through
  project result/reporting mechanisms; the host decides how errors are
  presented.
- Actual: `Engine::load_scene` writes glTF import warnings to `std::clog`.
  The public API returns only `Result<std::unique_ptr<Scene>>`, so hosts cannot
  inspect warnings programmatically.
- Evidence: `Engine::load_scene` iterates `import_result.value().warnings` and
  writes `"Elf3D scene import warning: ..."` to `std::clog`.
- Risk: GUI hosts cannot surface warnings consistently, tests cannot assert
  public warning behavior through the facade, and command-line logging may be
  inappropriate for embedding applications.
- Recommended correction: Introduce a public load result/report type, expose a
  warning callback in `SceneLoadOptions`, or explicitly document `std::clog`
  warning behavior as a temporary 0.1.0 limitation.
- Should code change: Preferred yes
- Should docs change: Yes if not fixed for 0.1.0
- Suggested stage: Goal 4 or Goal 5
- Suggested commit: `feat: expose scene import warnings through the public API`

### AUD-005: `baseColorFactor` Alpha Is Not Imported Despite Broad README Wording

- Severity: Medium
- Classification: Documentation/API behavior mismatch
- Affected files: `README.md`, `modules/gltf/src/importer.cpp`,
  `modules/gltf/tests/gltf_importer_test.cpp`, `modules/renderer/src/renderer.cpp`
- Expected: The README statement that the glTF subset imports
  `baseColorFactor` should either include all four components or explicitly
  state that alpha is ignored because the stage is opaque-only.
- Actual: The importer copies RGB from glTF `baseColorFactor` but forces alpha
  to `1.0F`. The renderer material pass writes `fragment_color = vec4(..., 1.0)`.
  Tests currently expect imported alpha to be `1.0F` for a material whose glTF
  factor contains alpha below one.
- Evidence: `material_for` assigns `Color4{pbr.base_color_factor[0],
  pbr.base_color_factor[1], pbr.base_color_factor[2], 1.0F}`; renderer output
  alpha is forced to `1.0`; README says `baseColorFactor` without qualification.
- Risk: Host applications inspecting material descriptions may believe glTF
  alpha was preserved. Documentation overstates the imported material subset.
- Recommended correction: For 0.1.0, update docs to say RGB `baseColorFactor`
  is imported and alpha is ignored/opaque. Alternatively import the alpha value
  into `MaterialDescription` while documenting that rendering remains opaque.
- Should code change: Optional
- Should docs change: Yes
- Suggested stage: Goal 5 documentation
- Suggested commit: `docs: clarify opaque glTF base color alpha behavior`

### AUD-006: C++ DLL Compatibility Expectations Are Not Explicit Enough

- Severity: Medium
- Classification: Documentation gap
- Affected files: `README.md`, public headers under `include/elf3d`
- Expected: A public shared-library release should clearly state binary
  compatibility expectations.
- Actual: The public API is a C++ API using standard library types, templates,
  and move-only Pimpl classes across the DLL surface. The repository documents
  dynamic MSVC runtime usage but does not clearly state that the 0.1.0 C++ ABI
  is intended for compatible compiler/standard-library/CRT configurations, not
  a stable C ABI.
- Evidence: Public API uses `std::unique_ptr`, `std::filesystem::path`,
  `std::optional`, `std::span`, `std::string_view`, and `Result<T>`.
- Risk: Consumers may assume toolchain-independent ABI compatibility that the
  API does not provide.
- Recommended correction: Add release documentation that defines supported
  compiler/runtime compatibility for 0.1.0 and identifies any future C ABI as a
  separate design.
- Should code change: No for 0.1.0
- Should docs change: Yes
- Suggested stage: Goal 5 documentation
- Suggested commit: `docs: state Elf3D 0.1.0 C++ ABI compatibility limits`

### AUD-007: GPU Resource Destruction Depends on Host Shutdown Order

- Severity: Medium
- Classification: Lifetime limitation
- Affected files: `include/elf3d/viewport.h`, `include/elf3d/elf3d.h`,
  `modules/backend_opengl/src/device.cpp`, `README.md`
- Expected: GPU resources are destroyed before their graphics device/context,
  and shutdown constraints are clear.
- Actual: Public docs state that viewport and engine destruction require the
  owning graphics thread and a compatible OpenGL context. The OpenGL backend
  checks this and skips deletion if the context is unavailable.
- Evidence: OpenGL resource destructors check `can_destroy_objects()` before GL
  deletion; README and public comments require context-current destruction.
- Risk: Incorrect host shutdown can leak GL resources. The current behavior
  avoids invalid GL calls, but it can hide the shutdown error.
- Recommended correction: Keep the current contract, add a release-note
  limitation, and consider debug diagnostics for skipped GL deletion later.
- Should code change: Optional
- Should docs change: Yes
- Suggested stage: Goal 5 documentation
- Suggested commit: `docs: document OpenGL resource shutdown requirements`

### AUD-008: Release Documentation Set Is Incomplete

- Severity: Medium
- Classification: Missing release documentation
- Affected files: `docs/`, `README.md`
- Expected: A release candidate should include a verified project state,
  release notes, supported feature matrix, known limitations, validation matrix,
  and user-visible API/lifetime notes.
- Actual: `README.md`, `ARCHITECTURE.md`, `CODING_POLICY.md`, and
  `THIRD_PARTY.md` are present and useful, and Goal 3 added an audit
  validation matrix. There is still no verified project-state document,
  release notes, release snapshot, or focused API/usage guide.
- Evidence: Repository inventory lists no release-specific docs beyond the
  newly added audit inventory.
- Risk: Users cannot distinguish confirmed 0.1.0 behavior from future design
  intent, and release reviewers cannot trace validation status.
- Recommended correction: Add the Goal 5 documentation set after remediation
  and validation.
- Should code change: No
- Should docs change: Yes
- Suggested stage: Goal 5 documentation
- Suggested commit: `docs: add Elf3D 0.1.0 release documentation`

## 7. Release Blockers

The following items block a 0.1.0 release tag at this point:

- AUD-001: Missing `PROJECT_STATE_EN.md` or equivalent verified state baseline.
- AUD-008: Verified release documentation and release snapshot are incomplete.

AUD-004 may also block release if the intended 0.1.0 public API promises
host-visible import warnings rather than temporary `std::clog` diagnostics.
Manual visual viewer validation also remains required before a release tag.

## 8. Non-Blocking Issues

The following issues do not block a 0.1.0 release if documented accurately:

- AUD-005: glTF base color alpha is ignored/opaque.
- AUD-006: C++ DLL ABI compatibility limits need explicit release wording.
- AUD-007: OpenGL resource destruction requires correct host shutdown order.
- AUD-008: Release documentation set is incomplete until Goal 5.

## 9. Documentation Mismatches

- `README.md` says the glTF subset imports `baseColorFactor`, but implementation
  imports only RGB and forces material/render alpha to opaque.
- Public docs state the Engine must outlive Scenes and Viewports, but the
  failure mode for violating that order is not described and is not made safe
  for Scenes.
- Import warning behavior is not presented as a public API contract or
  documented limitation.
- The 0.1.0 C++ ABI compatibility expectations are not stated plainly enough
  for consumers of a DLL.
- The requested `PROJECT_STATE_EN.md` does not exist.

## 10. Deferred Design Questions

- Should `Engine::load_scene` keep returning only `Scene`, or should 0.1.x add
  a `SceneLoadResult` with warnings and import statistics?
- Should release documentation describe Scene destruction after Engine teardown
  as a best-effort safe no-op, or keep it strictly as an out-of-contract host
  lifetime violation despite the safer implementation?
- Should material alpha be stored even while the renderer remains opaque-only,
  or should material descriptions deliberately normalize alpha to `1.0F` until
  alpha rendering exists?
- What compiler, CRT, and standard library combinations are supported for the
  public C++ DLL in 0.1.0?
- Is a future C ABI needed for cross-toolchain/plugin-style consumption, or is
  the 0.1.x product explicitly C++/MSVC-hosted only?
- Should OpenGL skipped-delete shutdown cases produce debug diagnostics, or
  remain silent to avoid logging from low-level modules?

## 11. Recommended Remediation Sequence

1. Decide AUD-004.
   If warning visibility is in scope for 0.1.0, add a public warning/report
   path and tests. If not, document `std::clog` warning behavior as a temporary
   limitation.

2. Update documentation for AUD-001, AUD-003, AUD-005, AUD-006, AUD-007, and AUD-008.
   This should produce a verified project-state document, release notes,
   feature matrix, known limitations, ABI/lifetime notes, and validation matrix.

3. Rerun the full validation workflow after documentation and any remaining
   remediation:

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release
```

4. Launch `elf3d_viewer` after a successful build and manually verify:
   startup cube rendering, model load, failed-load preservation, navigation,
   picking/selection, measurement, clipping, hierarchy visibility, reload, and
   close-scene behavior.

5. Only after remediation, documentation, build/test success, manual viewer
   validation, clean diff review, and release-condition review should an
   annotated `v0.1.0` tag be created.
