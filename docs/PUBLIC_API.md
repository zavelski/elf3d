# Elf3D C++ API Guide

Elf3D exposes a C++20 API through the headers in `include/elf3d`. The primary
include is:

```cpp
#include <elf3d/elf3d.h>
```

Build the `elf3d` target and link the resulting shared library and import
library with a compatible Visual Studio C++20 configuration.

## Creating the Engine

A rendering engine requires a current OpenGL context and a host-provided
procedure loader:

```cpp
elf3d::EngineConfiguration configuration;
configuration.graphics_backend = elf3d::GraphicsBackend::opengl;
configuration.opengl.load_procedure = load_host_opengl_symbol;

auto engine_result = elf3d::Engine::create(configuration);
if (!engine_result) {
    report_error(engine_result.error());
    return;
}
auto engine = std::move(engine_result).value();
```

A default-created engine can manage CPU scene data. Rendering viewports require
the configured graphics backend.

## Loading a Scene

```cpp
auto loaded_result = engine->load_scene_with_report("model.glb");
if (!loaded_result) {
    report_error(loaded_result.error());
    return;
}

auto loaded = std::move(loaded_result).value();
auto scene = std::move(loaded.scene);
for (const elf3d::SceneLoadDiagnostic& diagnostic : loaded.diagnostics) {
    present_diagnostic(diagnostic);
}
```

Use `load_scene()` when the compatibility report is not required.

## Creating and Rendering a Viewport

```cpp
auto viewport = engine->create_viewport({1280, 720}).value();
auto camera = scene->create_perspective_camera({}).value();

while (host_running()) {
    elf3d::ViewportInput input = translate_host_input();
    (void)viewport->update_navigation(*scene, camera, input);
    (void)viewport->render(*scene, camera);

    auto texture =
        engine->native_texture_view(viewport->color_texture()).value();
    present_texture(texture);
}
```

The host owns the native window, OpenGL context, event loop, input translation,
and texture presentation.

## Main API Areas

- `Engine`: scene and viewport creation, loading, and native texture access.
- `Scene`: hierarchy, transforms, cameras, assets, visibility, bounds, and
  statistics.
- `Viewport`: rendering, navigation, picking, selection, visibility,
  measurement, clipping, overlays, and statistics.
- `Result<T>` and `Error`: expected operation results and error context.

## Ownership and Shutdown

Elf3D objects are returned as `std::unique_ptr`. The engine must outlive every
scene and viewport created from it.

Shutdown in this order:

1. stop viewport operations;
2. destroy viewports while their OpenGL context is current;
3. destroy scenes;
4. destroy the engine;
5. destroy the host OpenGL context and window.

Scene mutation, loading, rendering, navigation, picking, and graphics-resource
management are used from the owning application and graphics thread.
