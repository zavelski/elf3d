# Elf3D 0.1.0 Known Limitations

Purpose: Record supported boundaries and known limitations for the 0.1.0
publication candidate.

Applicable version: 0.1.0

Document status: Publication limitation register.

Last verified implementation commit: `eeb39cdb2a9e92e61001a00d11cbe1880716f921`

Implementation source paths: `include/elf3d`, `modules`, `facade/elf3d`,
`apps/viewer`, `docs/GLTF_SUPPORT.md`, `docs/RENDERING_PIPELINE.md`,
`docs/LIFETIME_AND_THREADING.md`

Known limitations: This document is itself the limitation register.

Related documents: `PROJECT_STATE_EN.md`, `AUDIT_SUMMARY.md`,
`VALIDATION_SUMMARY.md`, `RELEASE_CHECKLIST.md`

## Publication Blocker

- Full manual viewer interaction validation has not been performed. The viewer
  starts, renders the project-owned fixture, and shuts down normally through
  the window close path, but the complete GUI interaction matrix has not been
  exercised for publication.

## Public API and ABI

- The public surface is a C++ API, not a stable C ABI.
- Consumers need compatible compiler, standard library, and CRT settings.
- Public APIs expose standard library types including move-only facades,
  `std::filesystem::path`, `std::optional`, `std::span`, `std::string_view`,
  and `Result<T>`.
- Objects created by Elf3D must be destroyed through supported Elf3D APIs or
  public facade destructors.

## Lifetime and Threading

- The host owns the native window, event loop, OpenGL context, main loop,
  Dear ImGui, GUI, and final presentation.
- Scene mutation, viewport rendering, picking, and tool operations are
  single-threaded in 0.1.0.
- The creating `Engine` must outlive its `Scene` and `Viewport` objects.
- Viewport and engine destruction require the owning graphics thread and a
  compatible current OpenGL context.
- The OpenGL backend skips object deletion when context/thread preconditions
  are not met, which avoids invalid GL calls but can leak GPU resources after
  incorrect host shutdown.

## glTF Import

- Only a static glTF 2.0 subset is supported.
- Unsupported: animations, skins, morph targets, cameras, lights, sparse
  accessors, non-triangle primitive modes, mesh compression, KTX2, normal maps,
  occlusion maps, emissive maps, and runtime plugin extensions.
- Unknown required extensions are rejected.
- Optional extensions are not interpreted.
- Import warnings are written to `std::clog`; the public scene-load result does
  not expose warnings programmatically.
- `MASK` and `BLEND` alpha modes are treated as opaque warnings.
- `baseColorFactor` RGB is imported, but alpha is forced to opaque.

## Rendering

- Only the OpenGL 4.1 backend exists.
- Rendering is opaque-only.
- Lighting is a bounded directional-light metallic-roughness path, not a full
  physically based renderer.
- No shadow maps, environment lighting, post-processing, transparency sorting,
  or multi-backend abstraction is release-ready.

## Viewport and Tools

- One selected entity per viewport.
- One distance measurement per viewport.
- One section plane and up to three axis-aligned clipping boxes per viewport.
- Navigation is mouse-based; no touch, gamepad, first-person, or keyboard
  navigation mode is implemented.
- Tool behavior is covered by unit/component tests but not by complete manual
  viewer interaction validation in this publication-prep pass.

## Packaging, CI, and Performance

- SDK packaging is deferred for 0.1.0.
- GitHub Actions workflows are committed locally but have not run remotely.
- No benchmark numbers are claimed for 0.1.0.
- No external model corpus was validated.
