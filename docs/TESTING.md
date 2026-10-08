# Testing Elf3D

## Preset Contracts

After changing CMake options, presets, or product composition, validate all
automated profile mappings and the four manual component contracts:

```powershell
.\cmake\check-preset-contracts.ps1
```

The check configures isolated full and model-only multi-configuration trees below
`out/`, verifies required/forbidden targets, explicit testing options, header
coverage and directory filters, viewer startup/debugger metadata, and the four
independent manual source-project compositions. It also configures the external application,
checks its own test/startup target, public-only includes, CRT, output paths,
and Elf3D's default/explicit testing and viewer options. It removes temporary
trees after completion (`-KeepBuilds` retains them). It does not compile or
provide evidence of Visual Studio UI behavior.

The same gate checks XML solution folders and `DefaultStartup`, missing/duplicate
project paths, local project references, v145/x64 and CRT settings,
effective compiler/MSBuild versions, repeated configuration, and rejection of
unsupported toolchains. Nested solutions must use `.slnx` too. Focused parser
and discovery checks can run without compiling or accessing a private model:
`cmake/check-solution-contracts.ps1`.

Actual component builds and ownership checks run separately:

```powershell
.\cmake\check-win-components.ps1 -Build
```

This uses an isolated root below `out`, retains its build logs, builds both
configurations, tests graphics smoke, Clean/Rebuild ownership, incremental DLL
deployment, missing Debug prerequisites, toolchain compatibility, and a CPU-only
imported Model client. It does not modify the manual `win` workspace.

## Automated Tests

Configure, build, and run the Debug suite:

```powershell
cmake --preset windows-full
cmake --build --preset windows-debug --parallel
ctest --preset windows-debug --output-on-failure
```

Run the Release suite:

```powershell
cmake --preset windows-full
cmake --build --preset windows-release --parallel
ctest --preset windows-release --output-on-failure
```

Run the model-only suite:

```powershell
cmake --preset windows-model
cmake --build --preset windows-model-debug --parallel
ctest --preset windows-model-debug --output-on-failure
```

The suite covers the standard application lifecycle and failure paths,
normalized input and interaction arbitration, scene assets and surface anchors,
navigation, picking, independent viewport selection/visibility/clipping state,
generic overlays, viewer-owned Tools and Workflows, typed command
FIFO/replacement barriers, separate Component-state ownership,
preferences/browser behavior, viewer shell smoke behavior, rendering
preparation, viewport lifetime, the public API, model document and
asset-reference behavior, and OpenGL rendering. A separate external target
copies and adapts a Measurement Tool while seeing only public Elf3D headers and
targets; it does not link the viewer or use private implementation includes.

Graphics integration tests borrow hidden `Application` contexts. The OpenGL
smoke covers top-down RGBA8 readback, incorrect buffer sizes, image availability
before rendering and after resize, current display transform, and preservation
of native pixel-pack settings and PBO binding. The application smoke verifies
completed-frame indices/timing, failed frames, and exit without another render.
These optional graphics tests are absent when `ELF3D_BUILD_APP=OFF`.

The context-dependent `elf3d.render_quality_material_pixels` test renders a
generated white/dielectric/polished-metal/rough-metal scene and enforces the
high-contrast studio's luminance, clipping, and shaped-highlight criteria. It
also renders the environment-only scene at `0` and `+10` degrees and requires
measurable polished- and rough-metal highlight motion without excessive
diffuse motion.
The optional
`elf3d_render_quality_capture` executable can write a resolved PNG plus
path-free JSON camera/settings metadata for local comparisons.

`elf3d.studio_environment_bake` runs the offline baker in `--verify` mode. It
requires byte-for-byte equality with the checked-in v3 resource; the baker also
guards the v1-calibrated source energy, fixed resource size, and unchanged BRDF
LUT bytes.

The model-only suite stops before renderer, backend OpenGL, viewport, Standard
Application Framework, ImGui, GLFW, and viewer targets.
It covers Document construction and processing, all-scene glTF import,
glTF/GLB export, source-image and raw-metadata fidelity, and verifies from
generated CMake metadata that Scene/Assets and engine/UI targets were not
configured. Preset-local test scratch directories keep concurrent full and
model-only profiles independent.

The named `elf3d.scene_runtime_adapter_depth` regression exercises the
permanent iterative Document-to-Scene adapter with a 5,120-level hierarchy.
The `elf3d.measurement_copy_adapt` regression exercises the external
public-only Measurement Tool through a hidden Standard Application driver.
The viewer smoke verifies that the copied `DroidSans.ttf` asset is the default
presentation font, then hides and restores the docked 3D View before completing
its render sequence, guarding interaction-region recreation as well as launch
and teardown.

CI runs the full Debug suite and an independent model-only Debug suite for
every push and pull request. A weekly schedule and manual dispatch additionally
run both Release suites. All jobs use the project's current pinned CMake 4.4.3
baseline rather than testing older CMake compatibility. The standard hosted
Windows environment does not guarantee an OpenGL 4.1 runtime, so the
context-dependent application, viewer, rendering, and integration smoke tests
may report `Skipped`; that result is not evidence
of real rendering. A hard graphics gate requires a runner that guarantees a
compatible context.

After building, a focused group can be run with a CTest expression:

```powershell
ctest --preset windows-debug -R "elf3d\.(scene|picking)" --output-on-failure
ctest --preset windows-debug -R "elf3d\.(application_smoke|interaction_arbiter|viewer_(behavior|smoke))" --output-on-failure
ctest --preset windows-debug -R "elf3d\.measurement_copy_adapt" --output-on-failure
ctest --preset windows-debug -R "elf3d\.scene_runtime_adapter_depth" --output-on-failure
ctest --preset windows-model-debug -R "elf3d\.model_" --output-on-failure
```

## Viewer Check

Launch the checked-in smoke model:

```powershell
.\out\build\windows-full\bin\Debug\elf3d_viewer.exe `
    .\tests\fixtures\elf3d_smoke\elf3d_smoke.gltf
```

Check model loading, orbit/pan/dolly navigation, selection, visibility,
measurement, clipping, resize, and clean shutdown.

The smoke model and its license are stored in
`tests/fixtures/elf3d_smoke/`.

## Visual Studio Debugging Routes

Use the four manual solutions described in
[BUILDING.md](BUILDING.md#manual-visual-studio-solutions), with `Debug | x64`.
Build Dependencies, Engine, ImGui, then Viewer. Start F5 from the Viewer
solution. Its deployed DLL and PDB are in `win/viewer/bin/Debug`.
Set breakpoints on executable statements in these files; function names below
are navigation landmarks. F9 toggles a breakpoint, F10 steps over, F11 steps
into, and Shift+F11 returns to the caller. Inspect **Call Stack**, **Locals**,
and **Watch** while paused.

| Route | Breakpoints and what to inspect |
| --- | --- |
| Startup | `apps/viewer/src/main.cpp` (`WinMain`) РІвЂ вЂ™ `run_viewer_entry` in `viewer_runtime.cpp` РІвЂ вЂ™ `elf3d::run_application` in `framework/app/src/application.cpp` РІвЂ вЂ™ `detail::EngineAccess::create` in `facade/elf3d/src/engine.cpp` РІвЂ вЂ™ `ViewerApplication::start`. Inspect application options, window/context creation results, and the owned Engine/Scene/Viewport. |
| Frame | `ViewerApplication::update` and `build_ui` in `viewer_runtime.cpp`, then `Renderer::render` in `modules/renderer/src/renderer.cpp`. Inspect frame input, viewport extent, camera, render request, and returned statistics. Disable frequently hit frame breakpoints after studying one frame. |
| Model loading | Start with the absolute fixture argument or open the model from the viewer. Follow `load_model_scene` in `apps/viewer/src/viewer_assets.cpp` РІвЂ вЂ™ `Engine::load_scene` in `facade/elf3d/src/engine.cpp` РІвЂ вЂ™ `gltf::load_document` in `modules/gltf/src/importer_document.cpp`. Inspect the source path, `Result`, load report, and replacement Scene; the previous scene is retained on a failed load. |
| Shutdown | Close the window and stop in `ViewerApplication::stop` in `viewer_runtime.cpp`, `Scene::~Scene` in `facade/elf3d/src/scene.cpp`, and `Engine::~Engine` in `facade/elf3d/src/engine.cpp`. Verify application-owned UI/Viewport/Scene release before the framework releases Engine, graphics context, and window. |

For hidden or unresolved breakpoints, pause and open **Debug > Windows >
Modules**. Locate `elf3d.dll`, confirm that its loaded path is in this build's
`bin/Debug` directory, and check **Symbol Status** / **Symbol Load Information**
for its matching `elf3d.pdb`. Check the viewer's PDB as well. Use the PDB from
the exact binary build; a similarly named PDB from another application's build
is insufficient. Rebuild if source and symbols disagree. Engine implementation
breakpoints resolve against the DLL even though much of its code was compiled
in the internal module-group projects. If stepping skips library calls, check
the C++ **Just My Code** debugger option.

Manually record header-tree navigation, startup target, F5, an application
breakpoint and an engine breakpoint, readable local values, and clean shutdown
as pass/fail/unavailable. Generated-file checks and hidden-context smoke tests
do not establish that these Visual Studio interactions work in a user session.

## Standalone External-Application Smoke

Run the separate configure/build/test commands in
[BUILDING.md](BUILDING.md#external-application-with-shared-elf3d-sources).
Its `external_application_smoke` must be registered while Elf3D tests and viewer
are absent by default. The test creates a hidden application and Scene, allows
one UI/presentation frame, requests exit on the next update, and checks that
`stop` released the Scene. It returns skip code `77` only for an unavailable
graphics context/version during initialization before application startup;
other failures remain errors. CI performs this independent build in its
existing Windows full Debug job.
