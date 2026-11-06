# Elf3D 0.13.0

Elf3D 0.13.0 expands scene preparation, hierarchy inspection and model writing,
with larger default work portions and lower graphics setup overhead.

## Changes

- Prepare canonical Documents and derived runtime data on an application-owned
  CPU worker, then install the completed Scene through the creating Engine.
  CPU retirement can transfer detached ownership to an exclusive worker.
- Drive graphics creation and retirement explicitly with elapsed-time, upload,
  driver-call and work-unit allowances. Geometry, material textures, shader
  pipelines and built-in environment resources participate in readiness.
- Use shared defaults of 8 MiB for vertex packing, 32 MiB for individual
  transfers and a 20 ms service target. Group native setup operations, adopt
  decoded image storage and cache validation for unchanged Documents.
- Browse authored objects and mesh resources in separate Viewer tabs. Inspect
  unused retained resources, search and filter rows, and explicitly frame,
  select, hide, show or isolate active resource instances.
- Select and highlight canonical primitive instances independently of entity
  selection. View-local visibility, isolation and backface-culling settings are
  shared by drawing, picking and navigation queries.
- Capture revision-bound hierarchy metadata without retaining source payloads.
  Use the streaming Model file writer for owned packet exchange, progress,
  cancellation and transactional GLB/glTF output.
- Read the completed presentation timestamp from application frame statistics.

## Windows package

Extract `elf3d-viewer-0.13.0-windows-x64.zip` and keep `assets` beside
`elf3d_viewer.exe`. The archive contains Viewer, Cmd, `elf3d.dll`, guides and
license notices. Verify the archive against `SHA256SUMS.txt`.

```text
elf3d_cmd.exe --input model.glb --output rotated.glb --rotate 0 0 90
```

## Requirements and compatibility

- Windows x64 and Microsoft Visual C++ v14 x64 Redistributable at least as recent
  as MSVC 19.51. Viewer requires an OpenGL 4.1 core-profile driver; Cmd needs no
  graphics driver.
- Source builds require Visual Studio 2026, v145, Windows SDK 10.0.26100.0 and
  PowerShell 7.6.5 or newer. Build Dependencies first, then Engine and ImGui,
  then Viewer, using one matching Debug/Release configuration throughout.
- Public C++ structures and APIs have expanded. Rebuild consumers with matching
  sources, toolchain and shared CRT settings. The ZIP is a runnable application
  package rather than a binary C++ SDK.
- Service budgets are checked between bounded work units; one allocator or
  graphics-driver call may exceed the requested time target. Other platforms
  remain unvalidated portability targets.

Elf3D source is licensed under MIT. Third-party software retains its own terms
and notices in `THIRD_PARTY.md` and `third_party_licenses`.
