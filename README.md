# Elf3D

Elf3D is a portable C++20 3D visualization engine intended to be embedded as a
shared library in desktop host applications. The current graphics-foundation
stage establishes an OpenGL 4.1 backend, engine-owned off-screen viewport, and
native texture interoperation with the Dear ImGui reference viewer. It does not
yet render scene geometry.

## Initial targets

- `elf3d_core`: minimal internal static library containing version and result data.
- `elf3d_math`: internal GLM-backed conventions and value conversion.
- `elf3d_graphics`: minimal internal device and render-target abstraction.
- `elf3d_backend_opengl`: private GLAD/OpenGL 4.1 implementation.
- `elf3d_viewport`: off-screen viewport state and render-target ownership.
- `elf3d`: public shared library and Pimpl-based engine/viewport facades.
- `elf3d_imgui`: optional static Dear ImGui GLFW/OpenGL3 integration helper.
- `elf3d_viewer`: standalone reference application and graphical testbed.
- Public API, math-convention, and viewport-lifetime tests.

The engine library has no dependency on Dear ImGui or GLFW. GLM is internal to
`elf3d_math`, while GLAD and OpenGL types remain private to
`elf3d_backend_opengl`. The host application owns the window, event loop,
OpenGL context, Dear ImGui context, GUI, and frame presentation.

## Graphics initialization and lifetime

The host creates and makes an OpenGL 4.1 core context current, then passes a
generic procedure loader to Elf3D:

```cpp
elf3d::EngineConfiguration configuration;
configuration.opengl.load_procedure = load_opengl_procedure;

auto engine_result = elf3d::Engine::create(configuration);
```

The viewer adapts `glfwGetProcAddress`; GLFW types do not cross the Elf3D API.
Viewport creation, resize, render, native texture access, and destruction occur
on the engine's owning graphics thread with a compatible context current.
Viewports and the engine must be destroyed before the host destroys that
context.

Elf3D uses a right-handed world, column-major matrices, column vectors,
`matrix * vector` transformation, `parent_world * local` composition, and
radians for public angles unless an API explicitly says otherwise.

## Windows prerequisites

- Windows x64.
- Visual Studio 2022 with the **Desktop development with C++** workload.
- CMake 3.25 or newer.
- Git, used by CMake FetchContent.
- A graphics driver supporting an OpenGL 4.1 core-profile context.

## Configure and build

Run from a Visual Studio Developer PowerShell or another terminal where `cmake`
is available:

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
```

For Release:

```powershell
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release
```

Both presets use one generated Visual Studio solution in
`out/build/windows-debug`; build outputs are separated into `bin/<Config>` and
`lib/<Config>`.

## Run the viewer

After a Debug build:

```powershell
.\out\build\windows-debug\bin\Debug\elf3d_viewer.exe
```

The viewer creates one GLFW/OpenGL window with a Dear ImGui dockspace, main menu,
dockable `3D View`, status bar, About window, and optional Dear ImGui demo window.
The `3D View` displays the real RGBA8 texture cleared by `elf3d.dll`, with a
24-bit depth attachment and a clear-color control. The project intentionally
uses the official Dear ImGui `docking` branch pinned to an exact commit;
multi-viewport support is intentionally disabled.

## Current limitations

Scene representation, geometry rendering, glTF loading, cameras, navigation,
picking, and tools remain outside this stage. The viewport currently clears its
off-screen color and depth targets; it draws no triangles or scene content.
