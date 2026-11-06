# Elf3D Cmd

`elf3d_cmd` is a console application and a source example for the public CPU-only
Model SDK. It reads a glTF/GLB file into a `Document`, rotates it, validates
the result, and writes another glTF/GLB file. It needs no engine DLL, graphics
context, graphics driver, viewer assets, or desktop application framework.

## Usage

```text
elf3d_cmd --input <input.gltf|input.glb> --output <output.gltf|output.glb> --rotate <x> <y> <z>
elf3d_cmd --help
```

```powershell
elf3d_cmd --input "building.glb" --output "converted/rotated.gltf" --rotate 0 90 0
```

Create the destination directory first. Paths may contain spaces and Unicode.
The filenames must end in lowercase `.gltf` or `.glb`. Input and output must be
distinct files, including existing hard-link aliases. All three processing
options are required and may occur once, in any order. Angles are in degrees and accept
finite binary32 values with a dot decimal separator and optional exponent;
negative values are supported. `--help` is used alone.

## Rotation and fidelity

Positive rotations follow the right-hand rule about fixed model axes, first X,
then Y, then Z. The pivot is the center of the axis-aligned bounds of all
node-instanced geometry in the Document, calculated before rotation in world
coordinates. Parent transforms and every mesh instance contribute to those
bounds; cameras and meshes without node instances do not define the pivot.
The pivot is shared across scenes, rather than calculated independently for each scene.

The operation left-multiplies each parentless node's matrix by
`T(center) * Rz * Ry * Rx * T(-center)`, once across all scenes.
Shared roots are not rotated twice. Child matrices, reusable meshes,
geometry, materials, scene membership, and authored default-scene selection
are retained. Camera nodes move with their hierarchy. Geometry bounds returned
by `Document::bounds()` describe untransformed mesh data and remain unchanged;
the represented scene geometry rotates through node transforms. Nonzero rotation
requires node-instanced geometry and affine node matrices; otherwise processing fails.

A zero angle or an integer number of complete turns on every axis avoids mutation.
Angles are reduced modulo 360 before evaluation. Matrix edits retain imported
`extras` and opaque extension blocks on supported Document objects. Their values
stay unchanged; Cmd does not interpret coordinates inside custom JSON. Known
material-volume factors and thickness textures are represented by the SDK and
exported. Structural/resource edits performed by other applications still cause
`preserved_metadata_dropped_after_mutation`. This is a supported static-model
processor; animation,
skins, morphs, and other unsupported input follow the import diagnostics.
See [GLTF.md](GLTF.md) for the complete format contract. Review warnings before
using the result as a replacement for an authored asset.

## Output and diagnostics

The output extension selects the container. GLB embeds image streams. glTF
writes `<stem>.bin` and `<stem>.image_<n>.<png|jpg>` sidecars beside the main file.
Existing destination artifacts are replaced transactionally by the SDK.
Use a separate destination directory to preserve the complete input bundle:
distinct main paths do not prove that generated sidecars cannot overlap source
resources. This application does not delete obsolete sidecars from earlier exports.

The normalized writer uses the same readable formatting for glTF and GLB,
including retained metadata: tabs, LF, a target width of 140 columns, and
matrices arranged in four column-major groups. Long indivisible strings may
exceed that target. Generated floats use short round-trip decimal spellings.
Cmd enables `ModelNumericWritePolicy::clean_rotation`: coefficients within
`1e-7` of an integer are cleaned only in orthonormal matrix bases. Translation,
scaled/sheared bases, geometry buffers, and opaque metadata numeric tokens
remain unchanged. The SDK reports affected matrices with the informational
`matrix_rotation_cleaned` diagnostic. Exact identity transforms and empty
`extras` are omitted. All output is reconstructed from Document state using
this single normalized export path.

Help and a successful-save summary go to stdout. Diagnostics go to stderr,
including the stage, severity, code, message, and available source context.
Import warnings can describe generated normals or unsupported content. Export
reports source-image re-encoding and metadata loss. Errors prevent saving or
report a failed SDK output transaction. Recovery-backup paths in an SDK error
must be retained if restoration failed.

| Outcome | Exit status |
| --- | ---: |
| Help or successful processing, including warnings | 0 |
| Invalid arguments or conflicting input/output paths | 2 |
| Load, transformation, validation, or save failure | 1 |

Memory exhaustion and unexpected process-boundary failures use the SDK's fatal
path and return a nonzero process status.

## Build and reuse

The Windows release archive contains `elf3d_cmd.exe` alongside the viewer.
Both require the matching Microsoft Visual C++ x64 Redistributable. Only the
viewer requires an OpenGL driver and its companion DLL/assets.

For manual development, build CPU Dependencies and Engine's `elf3d_model`
project in the same configuration, then open `win/Elf3D-Cmd.slnx`.
Outputs are `x64/elf3d_cmd.exe` (Release) and `x64/elf3d_cmdD.exe` (Debug).
F5 without arguments prints usage. Cmd Clean/Rebuild affects its own outputs.
`tools/build.ps1 -Scope Model` builds CPU dependencies, Model and Cmd without
requiring graphics; `tools/test.ps1 -Profile Model -Build` validates them.

The application sources in `src/cmd` show the complete pipeline. Copy
`rotate_document.h` and `.cpp` into your application's source tree to adapt
the operation. No private headers or native graphics types are involved.
The function can leave earlier roots changed on failure; discard that document
instead of saving it. Borrowed views must not outlive or span document mutation.

```xml
<PropertyGroup>
  <Elf3DSourceRoot>$(MSBuildThisFileDirectory)..\elf3d\</Elf3DSourceRoot>
  <Elf3DComponent>Model</Elf3DComponent>
</PropertyGroup>
<Import Project="$(Elf3DSourceRoot)win\Elf3D.props" />
```

The static Model SDK supplies C++20, public headers, and its CPU link closure;
consumers inherit no internal include directory or PCH requirement. Keep your
compiler and C++ runtime compatible. [external_model](../examples/external_model)
is a separately built application demonstrating copied rotation source,
loading, validation, export, and reimport.

Camera metadata, aspect ratios, absent far planes, and camera/material names
are preserved by normalized export.

Supported RGB TIFF textures retain their original streams. Invalid source images
are disabled with a warning; an HTML error page declared as JPEG cannot be
reconstructed as an image. TIFF and EXR resources require corresponding format
support in the application reading the exported file.
