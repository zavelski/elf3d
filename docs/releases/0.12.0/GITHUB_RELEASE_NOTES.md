# Elf3D 0.12.0

Elf3D 0.12.0 adds in-memory Document editing and ownership transfer between the
CPU Model SDK and the standard desktop runtime.

## Changes

- `clone_document` creates a complete independent copy with fresh owner-scoped
  IDs, preserving supported geometry, resources, scenes, cameras and metadata.
- `snapshot_document` retains independent tables, geometry and metadata while
  sharing immutable image bytes. Either owner may be destroyed first;
  `replace_image` installs new bytes only in the edited owner.
- `Engine::create_scene_from_document` adopts a selected scene from an existing
  Document without a file round trip or canonical payload copy. Returned errors
  restore the input owner. `Scene::document` observes the retained canonical data;
  `Scene::take_document` extracts it and retires the runtime Scene.
- Editor import options retain supported detached nodes and resource tables or
  allow empty geometry. Authored child order survives import, and unsupported
  vertex streams receive explicit diagnostics.
- Fresh reconstructions can share an immutable image record and transfer
  metadata using caller-verified ID mappings. Primitive replacement has an
  explicit policy for operations whose retained metadata references were audited.
- Model IDs distinguish independently linked implementations. Static Model and
  shared runtime targets can coexist in a consumer without `ELF3D_STATIC`.
- `Application::close_requested` allows an application to defer a platform
  close request while it processes unsaved work, then approve normal shutdown.

## Windows package

Extract `elf3d-viewer-0.12.0-windows-x64.zip` and keep `assets` beside
`elf3d_viewer.exe`. The archive contains Viewer, Cmd, `elf3d.dll`, guides and
license notices. Verify the archive against `SHA256SUMS.txt`.

```text
elf3d_cmd.exe --input model.glb --output rotated.glb --rotate 0 0 90
```

## Compatibility

- Rebuild all C++ consumers: public APIs and Model ID layouts have changed.
  Mixed Model/runtime consumers require matching sources, compiler, shared CRT
  and iterator-debug settings. The Model SDK remains source-integrated; the
  archive is a runnable application package rather than a binary C++ SDK.
- Windows x64 requires a Microsoft Visual C++ v14 x64 Redistributable at least
  as recent as MSVC 19.51. Viewer requires an OpenGL 4.1 core-profile driver;
  Cmd has no graphics-driver requirement.
- Source builds use Visual Studio 2026 with v145, CMake 4.4.3 and PowerShell
  7.6.5 or newer. Other platforms remain unvalidated portability targets.
- Snapshots share CPU image bytes only. Runtime preparation remains lazy,
  and applications own history, worker scheduling and replacement commit policy.
  Metadata transfer does not interpret or remap arbitrary opaque JSON references.

Elf3D source is licensed under MIT. Third-party software retains its own terms
and notices in `THIRD_PARTY.md` and `third_party_licenses`.
