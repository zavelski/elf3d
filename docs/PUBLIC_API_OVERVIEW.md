# Public API Overview

Purpose: Describe the verified Elf3D 0.2.0 public C++ API and host integration
contract.

Applicable version: 0.2.0

Document status: Verified against public headers and validation on 2026-06-24.

Last verified Git commit: pending 0.2.0 release source commit

Implementation source paths: `include/elf3d`, `facade/elf3d/src/engine.cpp`,
`tests/public_api_test.cpp`

Known limitations: This is a C++ API, not a stable C ABI. The 0.2.0 DLL surface
uses standard library types and is intended for compatible compiler, standard
library, and MSVC runtime configurations.

Related documents: `LIFETIME_AND_THREADING.md`, `MODULE_MAP.md`,
`GLTF_SUPPORT.md`, `TESTING.md`

## Public Headers

- `include/elf3d/elf3d.h`: version functions, `Engine`, primary facade include.
- `include/elf3d/scene.h`: `Scene`, hierarchy snapshots, scene load options,
  cameras, entities, assets, visibility, bounds, statistics.
- `include/elf3d/viewport.h`: `Viewport`, rendering, navigation, picking,
  selection, visibility, measurement, clipping, lighting, statistics.
- `include/elf3d/assets.h`: asset handles and mesh, image, texture, sampler,
  material descriptions.
- `include/elf3d/graphics.h`: backend selection, OpenGL procedure loader,
  texture handle, native texture view.
- `include/elf3d/math/value_types.h`: public math/value types.
- `include/elf3d/navigation.h`: viewport input and orbit navigation settings.
- `include/elf3d/picking.h`: rays, pick options, pick hits, statistics.
- `include/elf3d/selection.h`: selection settings and snapshots.
- `include/elf3d/measurement.h`: measurement settings, snapshots, projected
  points, overlay primitive types.
- `include/elf3d/clipping.h`: section plane, clipping boxes, helper settings,
  clipping snapshots.
- `include/elf3d/core/api.h`: `ELF3D_API`.
- `include/elf3d/core/error.h`: `ErrorCode` and `Error`.
- `include/elf3d/core/result.h`: `Result<T>` and `Result<void>`.
- `include/elf3d/core/version.h`: `Version`.

## Exported API Surface

Global API:

- `elf3d::version() noexcept`
- `elf3d::version_string() noexcept`

Exported classes:

- `elf3d::Engine`
- `elf3d::Scene`
- `elf3d::Viewport`
- `elf3d::SceneHierarchySnapshot`

The exported classes are move-only Pimpl facades. Copying is disabled. Objects
created by Elf3D are destroyed through their exported destructors by owning
`std::unique_ptr` values returned from the API.

## Engine Lifecycle

The host owns the native window, event loop, OpenGL context, Dear ImGui context,
GUI construction, main loop, and final presentation.

To create a rendering engine, the host must first create and make current an
OpenGL 4.1 core-compatible context and provide an OpenGL procedure loader:

```cpp
elf3d::EngineConfiguration configuration;
configuration.graphics_backend = elf3d::GraphicsBackend::opengl;
configuration.opengl.load_procedure = [](const char *name) -> void * {
    return load_host_opengl_symbol(name);
};

elf3d::Result<std::unique_ptr<elf3d::Engine>> engine_result =
    elf3d::Engine::create(configuration);
```

`Engine::graphics_initialized()` reports whether the facade has a graphics
device. A default-constructed `Engine` can create CPU scenes but has no graphics
backend and cannot create viewports.

## Scene API

`Engine::create_scene()` creates an empty logical scene. `Engine::load_scene()`
synchronously imports a glTF or GLB file into a new scene and returns that scene
only after successful import. Failed loads leave existing caller-owned scenes
unchanged.

`Scene` owns:

- entity identity and hierarchy
- local transforms and explicit local matrices
- perspective cameras
- model bindings
- persistent visibility
- CPU mesh, image, texture, sampler, and material assets
- hierarchy snapshots and statistics

Scene handles carry scene identity. Destroyed entity slots are not reused in the
current implementation.

## Viewport API

`Engine::create_viewport()` creates an off-screen viewport with an internal
render target. A viewport observes a caller-provided scene and camera; it does
not own the logical scene.

`Viewport` supports:

- resize and render
- clear color and basic directional lighting
- orbit/pan/dolly navigation
- fit and reset to visible content
- picking rays and viewport pick queries
- single selection per viewport
- persistent scene visibility commands for the selected entity
- per-viewport isolation
- one distance measurement per viewport
- one section plane and up to three clipping boxes per viewport
- neutral overlay lines and markers for tools
- render, picking, and measurement statistics

`Engine::native_texture_view()` returns a non-owning native texture view for a
viewport color texture. The host must never delete that texture.

## Error Model

Expected failures are reported through `Result<T>` or `Result<void>`.
`Error` contains an `ErrorCode` and fixed-size message. Public facade methods
catch unexpected exceptions at boundary points and translate them to structured
errors where practical.

Normal absence uses `std::optional`, for example no pick hit or no selected
entity.

Import warnings currently go to `std::clog` from `Engine::load_scene()`. They
are not returned through a public load report in 0.2.0.

## Thread and ABI Notes

Scene mutation and rendering are single-threaded in 0.2.0. Viewport creation,
resize, render, native texture access, and destruction are graphics-thread
operations and require a compatible current OpenGL context.

The public ABI uses standard library types including `std::unique_ptr`,
`std::filesystem::path`, `std::optional`, `std::span`, `std::string_view`, and
`Result<T>`. Consumers should build with a compatible MSVC toolchain and
dynamic runtime (`/MDd` for Debug, `/MD` for Release).

## Minimal Integration Flow

```cpp
auto engine = elf3d::Engine::create(configuration).value();
auto scene = engine->load_scene("model.glb").value();
auto viewport = engine->create_viewport({1280, 720}).value();

elf3d::EntityId camera = scene->create_perspective_camera({}).value();

while (host_running) {
    elf3d::ViewportInput input = translate_host_input();
    (void)viewport->update_navigation(*scene, camera, input);
    (void)viewport->render(*scene, camera);
    auto native = engine->native_texture_view(viewport->color_texture()).value();
    present_native_texture(native);
}

viewport.reset();
scene.reset();
engine.reset();
```

The example omits error handling for brevity. Production hosts should inspect
every `Result<T>`.
