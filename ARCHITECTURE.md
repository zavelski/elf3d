# Elf3D Architecture

## Document Status

This document defines the intended architecture of the Elf3D project.

It describes stable architectural boundaries, dependency directions, ownership
rules, binary composition, extension points, and the role of the reference
application.

Implementation details may evolve, but changes that violate the architectural
invariants in this document require an explicit architectural decision.

`CODING_POLICY.md` defines how code is written. This document defines where code
belongs, which dependencies are permitted, how the major parts of the system
cooperate, and which responsibilities remain outside the engine.

## 1. Project Definition

Elf3D is a portable C++20 3D visualization engine delivered primarily as an
embeddable shared library.

The project is conceptually inspired by the 3D part of Leadwerks:

- direct high-level access to common 3D concepts;
- a small and understandable object model;
- simple scene loading;
- simple camera and viewport control;
- explicit update and rendering;
- minimal ceremony for ordinary visualization tasks.

Elf3D is not intended to reproduce the complete Leadwerks engine or copy its
API exactly.

The initial purpose of Elf3D is to load, represent, display, navigate, inspect,
and later modify glTF scenes in desktop applications.

The engine is designed to operate inside a host application, including an
application that uses Dear ImGui as its GUI.

## 2. Architectural Goals

Elf3D should provide:

- a simple public C++ API;
- one primary shared library for host applications;
- clear internal module boundaries;
- explicit ownership and deterministic resource lifetime;
- no hidden application or event loop;
- no dependency of the engine core on Dear ImGui;
- portable scene and domain data;
- isolated graphics backends;
- support for one or more independently controlled 3D viewports;
- a path from built-in modules to optional runtime plugins;
- independently testable subsystems;
- a reference application that demonstrates the real public API;
- an architecture that remains understandable to ordinary C++ developers and
  AI-assisted development tools.

The project prefers useful modularity over both extremes:

- one unrestricted source-level monolith;
- excessive fragmentation into many small DLLs or artificial interfaces.

## 3. Non-Goals

The initial engine is not intended to provide:

- its own operating-system event loop;
- its own application main loop;
- its own GUI toolkit;
- a replacement for Dear ImGui;
- a complete application framework;
- a complete game engine;
- gameplay systems;
- physics simulation;
- audio;
- Steam integration;
- networking;
- scripting;
- an entity-component-system framework;
- a full scene editor;
- a universal plugin framework;
- a generic asset-management application;
- project-file management;
- runtime replacement of every subsystem;
- binary compatibility between unrelated C++ toolchains.

Features are added only when they support actual Elf3D use cases.

## 4. Project Products

The repository produces several targets with different responsibilities.

### 4.1 `elf3d`

`elf3d` is the public shared library:

```text
Windows:  elf3d.dll
Linux:    libelf3d.so
macOS:    libelf3d.dylib
```

It is the primary engine product.

The host application normally links only against:

- the Elf3D import library where required;
- public Elf3D headers;
- optional public integration libraries.

The shared library:

- exposes the supported public engine API;
- owns the composition root;
- creates and coordinates engine services;
- hides internal OBJECT-library and named-module implementation boundaries;
- hides graphics backend implementation details;
- contains no `main()` function;
- starts no event loop;
- creates no application GUI.

### 4.2 Internal engine modules

Major engine subsystems are grouped as internal CMake OBJECT libraries with
matching project-owned C++20 named-module interfaces.

These internal boundaries provide:

- source and dependency boundaries;
- independent build targets;
- focused tests;
- optional build composition;
- reduced compile-time coupling;
- a possible migration path to runtime plugins where justified.

They are linked into `elf3d` as object code.

They are not distributed as separate public SDK libraries unless a future
requirement explicitly changes that policy.

### 4.3 `elf3d_imgui`

`elf3d_imgui` is an optional integration library.

It may depend on:

- Dear ImGui;
- GLFW;
- the viewer's OpenGL presentation path.

It provides only small integration helpers where they simplify host code.

It is not part of the engine core and is never a dependency of `elf3d`.

### 4.4 `elf3d_viewer`

`elf3d_viewer` is the reference host application and graphical testbed.

It is:

- a demonstration of the public Elf3D API;
- the first executable integration client;
- a manual and visual test application;
- a smoke-test target;
- a place to demonstrate new engine capabilities as they are added.

The viewer is not intended to become a separate general-purpose editor.

Its GUI may grow to expose engine capabilities, but its purpose remains
demonstration, inspection, validation, and testing of Elf3D.

## 5. Host-Driven Execution Model

Elf3D uses a host-driven frame model.

The host application owns:

- native windows;
- the operating-system event loop;
- the application main loop;
- the Dear ImGui context;
- GUI construction;
- application commands;
- frame timing;
- frame presentation;
- application startup and shutdown.

The engine owns:

- engine state;
- scenes;
- viewports;
- rendering services;
- graphics resources;
- navigation state;
- picking state;
- registered tools;
- asset services.

The engine never starts a hidden loop.

A typical host loop is conceptually:

```cpp
while (!application_should_close()) {
    platform.poll_events();

    imgui.begin_frame();

    build_application_gui();

    viewport.resize(viewport_size);
    viewport.update(viewport_input, frame_info);
    viewport.render(scene);

    display_viewport_output(viewport.color_output());

    imgui.render();
    platform.present();
}
```

The exact API may evolve, but execution must remain initiated by explicit host
calls.

## 6. Dear ImGui Integration

Dear ImGui belongs to the host layer.

The project intentionally uses the official Dear ImGui `docking` branch for the
reference viewer and pins the selected revision to an exact commit.

The engine shared library must not:

- include Dear ImGui headers;
- use `ImGuiContext`;
- use `ImGuiIO`;
- expose `ImTextureID`;
- create dockspaces;
- create application windows or panels;
- read Dear ImGui global input state.

The viewer or integration adapter translates Dear ImGui state into
engine-defined structures.

The intended direction is:

```text
Dear ImGui
    ↓
elf3d_imgui or viewer adapter
    ↓
Elf3D public input and viewport API
    ↓
Engine subsystems
```

The primary embedded rendering path is:

```text
Elf3D viewport
    ↓
off-screen color texture
    ↓
host integration adapter
    ↓
ImGui::Image
```

Dear ImGui multi-viewport support is not required by the initial architecture.
Docking support is enabled in the viewer from the first stage.

## 7. High-Level System Overview

```text
Host Application or elf3d_viewer
                │
                ├── native window and event loop
                ├── Dear ImGui
                ├── application commands
                └── frame presentation
                │
                ▼
          Elf3D Public Facade
                │
        ┌───────┼────────┐
        ▼       ▼        ▼
      Scene   Viewport   Assets
        │       │        │
        └───┬───┴────────┘
            ▼
         Renderer
            │
            ▼
     Graphics Abstraction
            │
            ▼
      Graphics Backend
```

Interaction and tool modules cooperate with the viewport, scene-query, and
render-overlay interfaces without depending on the application GUI.

## 8. Binary and Module Model

Elf3D uses a modular-monolith binary model.

The normal deployment contains:

```text
elf3d.dll
elf3d_viewer.exe
```

The shared library is assembled from a reasonable number of internal OBJECT
libraries.

A future deployment may also contain optional runtime plugin DLLs.

The three concepts are distinct:

### 8.1 Logical module

A coherent architectural subsystem, such as Scene, Navigation, or Picking.
Project-owned C++20 named modules express the current internal engine OBJECT
library boundaries.

### 8.2 Build module

A separate CMake target, usually an internal OBJECT library for built-in engine
code.

### 8.3 Runtime plugin

A separate shared library loaded dynamically and versioned independently.

Not every logical module needs its own build target.

Not every build module should become a DLL.

Current built-in engine OBJECT libraries expose C++ named-module interfaces and
module implementation units. Internal users import these modules directly;
legacy import-only shim headers are not retained as a second internal surface.

A separate module is justified when it provides at least one substantial
benefit:

- a clear independent responsibility;
- independent testing;
- an isolated third-party dependency;
- an optional build feature;
- a platform or graphics backend boundary;
- reduced compile-time coupling;
- a realistic future runtime-plugin boundary.

Module count is not a goal by itself.

## 9. Planned Module Responsibilities

Modules are introduced only when they contain real functionality. The list below
defines intended boundaries and does not require empty targets to be created in
advance.

### 9.1 `elf3d_core`

`elf3d_core` is the smallest foundation module.

It may contain:

- build and platform configuration;
- shared-library symbol visibility;
- engine version data;
- the common `Error` and `Result<T>` mechanism;
- assertions;
- the logging contract;
- typed identifiers and handles;
- minimal module metadata;
- a small number of proven common utilities.

It must not contain:

- Scene;
- Entity;
- Renderer;
- graphics resources;
- glTF parsing;
- Dear ImGui;
- navigation;
- picking;
- tools;
- plugin loading;
- application framework code.

`elf3d_core` should remain small, stable, and low in the dependency graph.

### 9.2 `elf3d_math`

`elf3d_math` provides mathematical primitives and geometry required by multiple
engine modules.

Expected concepts include:

- 2D, 3D, and 4D vectors;
- matrices;
- quaternions;
- transforms;
- bounds;
- rays;
- planes;
- frustums;
- projection helpers.

GLM may be used internally by this module.

GLM types and configuration must not become part of the public DLL ABI.

The public Elf3D API uses Elf3D-owned value types.

The project convention is that one world-space unit equals one meter. Systems
that report physical distances use meters as their canonical value and may
leave display-unit formatting to the host or a narrow tool helper.

### 9.3 `elf3d_scene`

`elf3d_scene` owns the logical scene representation.

Expected responsibilities include:

- scene lifetime;
- stable scene and entity identifiers;
- node or entity hierarchy;
- local and world transforms;
- model instances;
- mesh references;
- material references;
- cameras;
- lights;
- visibility;
- scene bounds;
- change tracking required by rendering.

Scene data must remain independent of Dear ImGui and one concrete graphics
backend.

The scene must not depend on the renderer.

The scene must not own or depend on Viewport clipping state. Scene visibility
and Viewport clipping are separate filters that are composed by higher-level
Viewport operations.

### 9.3.1 `elf3d_clipping`

`elf3d_clipping` provides neutral low-level clipping data and algorithms.

Responsibilities include:

- section-plane normalization and half-space evaluation;
- world-axis-aligned clipping-box validation;
- combined point-filter evaluation;
- conservative bounds classification;
- conservative clipped-bounds calculation;
- fixed-size renderer-friendly clipping data.

It may depend on `elf3d_core` and `elf3d_math`.

It must not depend on:

- Scene;
- Renderer;
- Picking;
- Viewport tool state;
- Dear ImGui;
- GLFW;
- native graphics APIs.

### 9.4 `elf3d_assets`

`elf3d_assets` owns engine-side asset data and resource descriptors.

Expected responsibilities include:

- mesh data;
- image data;
- material data;
- asset identifiers;
- asset lifetime;
- validation of imported resources;
- CPU-side resource preparation.

Asset ownership and GPU resource ownership are separate concerns.

### 9.4.1 `elf3d_image`

`elf3d_image` performs bounded PNG/JPEG decoding into Elf3D-owned RGBA8
values. It isolates stb_image, has no glTF dependency, and exposes no stb types.
glTF resolves encoded image sources and calls this module; Assets owns the
resulting decoded pixels.

### 9.5 `elf3d_gltf`

`elf3d_gltf` imports glTF and GLB data.

The initial parser is expected to use `cgltf`, isolated inside this module.

Responsibilities include:

- parsing `.gltf` and `.glb`;
- resolving buffers and images;
- validating accessors, counts, offsets, and indices;
- converting glTF data into Elf3D scene and asset data;
- preserving supported metadata and extensions;
- reporting structured import errors.

The rest of the engine must not depend on `cgltf` types.

The scene module must not know that glTF is the primary input format.

### 9.6 `elf3d_graphics`

`elf3d_graphics` defines the portable low-level graphics abstraction.

Expected concepts include:

- graphics device;
- buffers;
- textures;
- samplers;
- shaders;
- pipelines;
- render targets;
- command submission;
- backend-neutral synchronization concepts.

The abstraction should remain as small as the real renderer requires.

It must not attempt to model every feature of every graphics API in advance.

### 9.7 Graphics backend modules

A graphics backend implements `elf3d_graphics` for one native API.

The initial backend is expected to target OpenGL 4.1.

Future backends may include Vulkan, Direct3D, or Metal where justified.

Only backend modules may depend directly on native graphics API headers.

Native graphics types must not spread into Scene, Assets, Navigation, or the
public application-facing domain API.

### 9.8 `elf3d_renderer`

`elf3d_renderer` converts scene and viewport state into graphics commands.

Expected responsibilities include:

- visibility evaluation;
- frustum culling;
- render-list creation;
- render-pass orchestration;
- material and pipeline selection;
- draw submission;
- lighting;
- off-screen rendering;
- diagnostic and overlay rendering.

The renderer may maintain render-specific caches and GPU representations.

It must not own or silently mutate the logical scene.

### 9.9 `elf3d_viewport`

`elf3d_viewport` represents one independently renderable view.

A viewport may own:

- dimensions;
- camera state or camera selection;
- render targets;
- navigation state;
- per-view render settings;
- picking resources;
- per-view diagnostic state.

Several viewports may display the same scene.

A viewport does not require shared ownership of its scene.

Persistent entity visibility belongs to `Scene` and affects every viewport that
observes that scene. Temporary subtree isolation belongs to one `Viewport`; it
does not rewrite Scene visibility and does not affect other viewports. Renderer,
picking, visible-bounds queries, and navigation fitting consume a neutral
visibility filter so they share the same visible-in-viewport definition.

Point-to-point measurement state also belongs to one `Viewport`. Measurement
anchors reference Scene geometry through stable scene, entity, mesh, primitive,
triangle, and barycentric identifiers; they do not own the Scene or copy mesh
geometry. The renderer consumes only neutral overlay line and marker primitives
produced from resolved measurement state. Dear ImGui, when present, renders the
screen-space measurement label in the host layer after projecting the
measurement midpoint through the public Viewport API.

Section and clipping state also belongs to one `Viewport`. A Viewport may own
one optional world-space section plane, up to three world-axis-aligned clipping
boxes, retained-side policy, helper visibility, helper styling, and a clipping
revision. Scene visibility, Viewport isolation, and Viewport clipping are
independent; Renderer, Picking, Measurement, visible-bounds queries, and camera
fit/reset consume one neutral clipping filter after visibility and isolation
are resolved. Clipping does not modify Scene hierarchy, Scene visibility, mesh
geometry, material data, picking BVHs, or measurement anchors.

### 9.10 `elf3d_interaction`

`elf3d_interaction` defines common interaction concepts used by navigation and
tools.

Expected concepts include:

- viewport input state;
- pointer capture;
- keyboard modifiers;
- interaction context;
- tool activation;
- interaction results;
- temporary interaction state.

It contains no Dear ImGui dependency.

### 9.11 `elf3d_navigation`

`elf3d_navigation` updates camera state from explicit input.

Expected navigation includes:

- orbit or examine;
- pan;
- zoom or dolly;
- fit to scene;
- focus on object;
- optional first-person navigation later.

Navigation does not process operating-system events directly.

Navigation does not query Dear ImGui state.

### 9.12 `elf3d_picking`

`elf3d_picking` converts viewport coordinates into scene queries.

Possible implementations include:

- CPU ray intersection;
- object-identifier render buffers;
- depth-based world-position reconstruction;
- hybrid techniques.

Picking returns stable scene identifiers and hit information.

It must not expose renderer-internal object pointers.

### 9.13 Tool modules

User-facing 3D tools are optional feature modules.

Examples include:

- selection;
- measurement;
- visibility;
- transform manipulation;
- section and clipping tools.

A tool module may contain:

- tool state;
- input handling;
- scene queries;
- temporary geometry;
- 3D overlay rendering;
- structured results and commands.

A tool must not create application GUI.

The host application may create Dear ImGui panels that configure and display the
state of an engine tool.

The initial distance-measurement tool is a built-in engine module. It stores one
in-progress or completed point-to-point measurement per Viewport, derives
anchors from existing picking results, resolves current world positions through
Scene and Asset queries, reports canonical distances in meters, and generates
backend-neutral overlay data. It must not depend on Dear ImGui, GLFW, OpenGL, or
private renderer implementation types.

The initial section/clipping tool is also a built-in engine module. It owns only
Viewport clipping state and commands, initializes boxes from current visible
Scene bounds without retaining Scene ownership, and generates backend-neutral
helper overlay primitives. Renderer and Picking consume only the neutral
`elf3d_clipping` filter; they must not depend on the clipping tool module.

### 9.14 Module and plugin infrastructure

Module registration and runtime plugin loading are separate concerns.

Built-in modules are registered explicitly by the shared-library composition
root.

Runtime loading, plugin discovery, ABI versioning, and unloading belong to a
dedicated plugin subsystem introduced only when a real runtime plugin is
implemented.

## 10. Dependency Direction

Dependencies must remain explicit and mostly acyclic.

The intended direction is approximately:

```text
elf3d_viewer
    ├── elf3d_imgui
    └── elf3d public API
             │
             ▼
       elf3d facade
             │
    ┌────────┼─────────────┐
    ▼        ▼             ▼
 viewport   assets       modules/tools
    │        │             │
    ▼        ▼             ▼
 renderer   gltf       interaction
    │        │         navigation/picking
    ▼        ▼
 graphics  scene
    │        │
    └────┬───┘
         ▼
        math
         │
         ▼
        core
```

This diagram expresses direction, not a requirement that every arrow correspond
to a direct link dependency.

Mandatory dependency rules:

- `core` depends on no higher-level engine subsystem;
- `math` may depend on `core`;
- `scene` may depend on `core` and `math`;
- `scene` must not depend on `renderer`;
- `gltf` may depend on scene and asset import interfaces;
- scene and assets must not depend on glTF;
- `renderer` may read scene and asset data;
- renderer-specific types must not leak back into Scene;
- scene must not depend on clipping;
- renderer and picking may depend on neutral `elf3d_clipping`;
- renderer and picking must not depend on `elf3d_tool_clipping`;
- graphics abstraction must not contain Scene policy;
- native backend types must remain inside backend boundaries;
- navigation and tools must not depend on Dear ImGui;
- the viewer depends on the engine, never the reverse;
- runtime plugins must not include private engine implementation headers;
- circular dependencies between major modules are prohibited.

## 11. Public API Design

The public C++ API uses:

```cpp
namespace elf3d {
}
```

Public types use ordinary names:

```cpp
elf3d::Engine
elf3d::Scene
elf3d::Viewport
elf3d::EntityId
elf3d::Float3
```

The API does not prefix C++ class names with `Elf_` or `ELF_`.

Global macros and build definitions use the `ELF3D_` prefix:

```cpp
ELF3D_API
ELF3D_ASSERT
ELF3D_VERSION_MAJOR
```

A future C-compatible API or plugin ABI uses the `elf3d_` function prefix and
`ELF3D_` enum-value prefix.

The public facade must hide:

- internal OBJECT-library and named-module boundaries;
- internal registries;
- concrete renderer classes;
- backend-specific classes;
- Dear ImGui types;
- GLFW types;
- GLM types;
- cgltf types;
- internal containers and caches.

The public API should expose domain-level operations, stable identifiers, small
value types, views, and facade objects.

## 12. DLL and ABI Policy

The primary public API may be a C++ API for applications built with a compatible
compiler, runtime library, standard library, architecture, and build
configuration.

Elf3D does not promise arbitrary cross-toolchain C++ ABI compatibility.

Public facade classes should use Pimpl or opaque handles where this meaningfully
reduces ABI exposure and dependency leakage.

Objects created by Elf3D must be destroyed through the supported Elf3D API or
through exported facade destructors.

The host must not directly delete private engine implementation objects.

Avoid exposing the following across a long-lived public DLL boundary:

- private STL container layouts;
- GLM types;
- backend-native types;
- internal inheritance hierarchies;
- private allocator-dependent objects.

A future stable C ABI may be introduced when support for unrelated compilers or
other programming languages becomes a real requirement.

C++20 module export does not export DLL symbols. Public DLL symbols must remain
controlled by explicit export/import declarations such as `ELF3D_API`.
Generated compiler module artifacts such as BMI, IFC, PCM, and GCM files are
build outputs, not ABI or SDK artifacts.

Exported project module interfaces must not expose third-party types or
third-party headers. Third-party dependencies remain isolated behind normal
headers, implementation files, and build targets.

## 13. Engine Object Model

The public object model is inspired by high-level 3D engines but remains smaller
than a game engine.

Initial central concepts are expected to include:

- `Engine`;
- `Scene`;
- `Entity` or stable `EntityId`;
- `Model`;
- `Camera`;
- `Light`;
- `Material`;
- `Texture`;
- `Viewport`;
- `RenderTarget`.

`Scene` is the preferred initial name for the logical 3D environment.

A separate `World` abstraction is not introduced unless a real distinction
later appears.

`Entity` is a spatial scene concept with identity and transformation.

The public API may present a convenient object-oriented model while the internal
implementation may use data-oriented storage where that improves performance
and remains understandable.

A deep inheritance hierarchy is not required.

Closed sets of scene alternatives may use value representations or variants.

Open extension points may use small runtime interfaces where required.

## 14. Ownership and Lifetime

The host application owns the root `Engine` object.

`Engine` owns long-lived shared services such as:

- graphics-device integration;
- renderer infrastructure;
- shared caches;
- shader and pipeline services;
- module registration;
- asset services.

The application or an application-level document normally owns each `Scene`.

A `Scene` owns its logical scene objects and stable identifiers.

A viewport observes a scene during update, picking, and rendering.

Multiple viewports may display the same scene without requiring
`std::shared_ptr<Scene>`.

The host must ensure that a scene outlives operations that reference it.

The renderer owns render-specific GPU representations and caches.

The scene owns logical data, not GPU objects.

Graphics resources must be destroyed before the graphics device or context that
created them.

The engine must be destroyed before the host destroys required native graphics
infrastructure.

Ownership transfer and non-owning observation must be explicit in the public
API.

## 15. Frame Flow

A normal frame follows this sequence:

1. The host processes operating-system events.
2. The host starts a Dear ImGui frame when ImGui is used.
3. The host builds application menus and panels.
4. The host determines the visible 3D viewport rectangle.
5. The host integration layer builds engine-defined viewport input.
6. The viewport updates navigation and active interaction state.
7. The engine synchronizes pending scene or asset changes as required.
8. The renderer renders the scene into the viewport render targets.
9. The integration layer exposes the viewport color output to Dear ImGui.
10. Dear ImGui renders the 3D image and surrounding GUI.
11. The host presents the final application frame.

The engine does not independently present the host application window.

## 16. Rendering Model

The primary embedded rendering path uses off-screen render targets.

A viewport may use:

- a color texture;
- a depth texture;
- an object-identifier texture;
- temporary post-processing textures;
- diagnostic overlay targets.

The host displays the final color output inside its GUI.

Direct rendering to a host framebuffer may be supported later, but it is not
the primary architecture for the ImGui viewer.

The initial rendering implementation should be deliberately small:

- perspective camera;
- depth testing;
- indexed triangles;
- node transforms;
- normals and `TEXCOORD_0`;
- base-color factor and texture;
- metallic-roughness factors and texture;
- one compact directional-light metallic-roughness PBR shader;
- ambient contribution;
- back-face culling;
- viewport resize;
- background clear.

Viewport clipping is implemented as fixed-size world-space clipping state:
Renderer broad-phase culls fully rejected model-primitive bounds, intersecting
geometry remains submitted, and the existing material shader discards fragments
that fail the neutral clipping filter. Clipping changes must not rewrite Scene
geometry, mutate materials, rebuild picking BVHs, or recreate immutable GPU mesh
and texture resources.

Image-based lighting, advanced material extensions, shadows, transparency,
animation, and post-processing are later features.

## 17. Input Model

The engine uses explicit frame-state input for ordinary viewport interaction.

An engine-defined input value may contain:

```cpp
struct ViewportInput {
    Float2 pointer_position;
    Float2 pointer_delta;
    float wheel_delta = 0.0f;

    bool hovered = false;
    bool focused = false;

    bool left_button_down = false;
    bool right_button_down = false;
    bool middle_button_down = false;

    bool shift_down = false;
    bool control_down = false;
    bool alt_down = false;
};
```

The exact type may evolve.

The host decides whether GUI input is captured by Dear ImGui or forwarded to a
3D viewport.

The engine may derive state transitions such as pressed, released, drag started,
or drag ended.

A general global event bus is not required for this interaction model.

## 18. Navigation and Tools

Navigation is a viewport behavior, not an application GUI feature.

The initial navigation contract should support:

- orbit or examine with mouse drag;
- pan with Shift plus mouse drag;
- optional middle-button pan;
- zoom or dolly with the mouse wheel;
- fit to scene;
- reset camera.

The navigation pivot initially uses the center of scene bounds.

Tools use a small engine-owned contract and receive explicit interaction and
rendering contexts.

Only one exclusive viewport tool is normally active at a time.

Passive features such as selection highlighting or diagnostics may coexist with
an active tool.

The initial active Viewport tool set is deliberately small: selection and
distance measurement. Navigation keeps priority over active tools for orbit,
pan, and zoom; a plain click below the drag threshold is routed to the active
tool on release. Clipping is a persistent per-Viewport feature rather than an
exclusive pointer tool; its public commands may be driven by a host panel while
ordinary navigation, selection, and measurement remain explicit.

The first likely tool modules are:

1. selection;
2. measurement.

These should first be implemented as built-in engine modules.

A runtime DLL version should be attempted only after the tool contract has
stabilized.

## 19. Asset and glTF Flow

The initial asset flow is:

```text
.gltf or .glb
      ↓
cgltf parser inside elf3d_gltf
      ↓
validated import representation
      ↓
Elf3D Scene and Assets
      ↓
renderer resource preparation
      ↓
GPU resources
```

The importer must validate untrusted external data before it enters trusted
engine structures.

The initial glTF scope should include:

- `.gltf` and `.glb`;
- external and embedded buffers;
- PNG and JPEG images;
- node hierarchy;
- transforms;
- meshes and primitives;
- indexed and non-indexed triangles;
- `POSITION`;
- `NORMAL`;
- `TEXCOORD_0`;
- base-color factor;
- base-color texture.
- metallic and roughness factors;
- metallic-roughness texture.

Animation, skins, morph targets, compression extensions, KTX2, and advanced
material extensions are introduced separately.

The architecture must allow the glTF importer implementation to change without
changing the public scene API.

## 20. Built-In Module Registration

Built-in engine modules are registered explicitly by the `elf3d` composition
root.

Conceptually:

```cpp
void register_builtin_modules(ModuleRegistry& registry)
{
    register_gltf_module(registry);
    register_navigation_module(registry);
    register_picking_module(registry);
    register_selection_tool_module(registry);
    register_measurement_tool_module(registry);
    register_default_graphics_backend(registry);
}
```

Hidden registration through global constructors or unreferenced static objects
is prohibited.

Explicit registration provides:

- deterministic startup;
- visible build composition;
- predictable shutdown;
- reliable object-library composition;
- easier testing;
- no dependency on linker-specific whole-archive behavior for registration.

The composition root is the only place that needs to know the full built-in
module set.

## 21. Runtime Plugin Model

Runtime plugins are optional separate shared libraries.

They are not required for the initial engine version.

Potential plugin use cases include:

- specialized tools;
- proprietary extensions;
- optional import formats;
- optional rendering features;
- independently distributed integrations.

A runtime plugin is justified only when independent deployment, licensing,
selection, or replacement provides a concrete benefit.

Runtime plugins require a narrow versioned ABI.

The preferred plugin boundary is C-compatible and uses:

- explicit ABI version;
- structure sizes;
- function pointers;
- opaque context pointers or integer handles;
- explicit creation and destruction;
- no exception propagation;
- no GLM types;
- no private STL containers;
- no private implementation headers.

Conceptually:

```cpp
extern "C" {

ELF3D_PLUGIN_API std::uint32_t
elf3d_plugin_abi_version();

ELF3D_PLUGIN_API bool
elf3d_plugin_create(
    const elf3d_host_api* host_api,
    elf3d_plugin_api* plugin_api);

}
```

Plugin unloading must not be supported until the engine can prove that no
plugin-owned objects, callbacks, registrations, tasks, or GPU resources remain.

## 22. Threading Model

The initial engine uses a simple single-threaded frame model.

The main thread normally performs:

- application GUI;
- viewport update;
- graphics command submission;
- Dear ImGui rendering;
- final presentation.

Background work may later be introduced for:

- file reading;
- image decoding;
- glTF parsing;
- mesh preparation;
- shader preparation where supported;
- expensive scene preprocessing.

Background workers must not directly mutate:

- Dear ImGui state;
- thread-confined graphics contexts;
- active viewport interaction state;
- scene data being read concurrently without synchronization.

Completed background work is transferred explicitly to the owning thread.

Concurrency is introduced only after a concrete requirement and a clear
ownership, cancellation, completion, and shutdown model exist.

## 23. Error and Diagnostic Boundaries

Low-level and engine modules return structured errors.

They do not:

- create GUI dialogs;
- show Dear ImGui popups;
- decide application-level recovery policy.

The host decides whether an error is:

- displayed;
- logged;
- retried;
- ignored;
- treated as fatal.

Exceptions must not escape:

- `main()`;
- C ABI boundaries;
- runtime plugin ABI boundaries;
- thread-entry functions;
- foreign callback boundaries that prohibit exceptions.

Logging uses a host-configurable engine contract.

The core logging interface must not depend on Dear ImGui.

The viewer may provide an ImGui-based log display as a host feature.

## 24. Reference Viewer Architecture

The viewer owns:

- GLFW initialization;
- the native window;
- the OpenGL context used for GUI presentation;
- Dear ImGui initialization;
- the Dear ImGui docking configuration;
- the main dockspace;
- application menus and panels;
- file selection;
- application-level commands;
- error presentation;
- the outer frame loop;
- final presentation.

The first viewer stage contains:

- one native window;
- a main dockspace;
- a main menu;
- one dockable `3D View` window;
- a status bar;
- an About window;
- the optional Dear ImGui demo window.

The initial menu contains:

```text
File
    Open...
    Exit

View
    3D View
    Dear ImGui Demo
    Status Bar

Help
    About Elf3D
```

Before the renderer exists, the `3D View` displays a clear placeholder.

As the engine develops, the viewer may add panels for:

- scene hierarchy;
- model information;
- render statistics;
- cameras;
- lighting;
- materials;
- selection;
- measurement;
- visibility;
- render modes;
- diagnostics;
- performance;
- animation.

Each addition must demonstrate real engine functionality.

The viewer must not implement engine behavior that belongs in the library.

## 25. Testbed Rule

The reference viewer must exercise the supported public engine API.

It must not depend on:

- private engine headers;
- internal engine OBJECT module targets;
- private renderer classes;
- internal scene storage;
- hidden resource caches;
- backend-only implementation types.

If a viewer feature cannot be implemented through the public API, first
determine whether the public API is incomplete.

Private diagnostic hooks are allowed only when the information is genuinely
diagnostic and should not become part of ordinary engine usage.

## 26. Repository and Build Structure

Elf3D uses:

- one Git repository;
- one Codex project rooted at the repository;
- one root CMake project;
- one generated Visual Studio solution;
- multiple CMake targets.

A target-oriented repository structure is expected:

```text
Elf3D/
├── CMakeLists.txt
├── CMakePresets.json
├── AGENTS.md
├── CODING_POLICY.md
├── ARCHITECTURE.md
├── README.md
├── THIRD_PARTY.md
│
├── cmake/
│
├── include/
│   └── elf3d/
│
├── modules/
│   ├── core/
│   ├── math/
│   ├── scene/
│   ├── assets/
│   ├── gltf/
│   ├── graphics/
│   ├── renderer/
│   ├── viewport/
│   ├── interaction/
│   ├── navigation/
│   ├── picking/
│   └── tools/
│
├── facade/
│   └── elf3d/
│
├── integrations/
│   └── imgui/
│
├── apps/
│   └── viewer/
│
├── plugins/
│
├── tests/
│
└── third_party/
```

Directories and targets are created only when they contain real implementation.

CMake is the authoritative build system.

The generated solution is not maintained manually.

The viewer links against the public `elf3d` target and optional integration
targets.

Module tests may link directly against internal OBJECT-library groups and their
object files.

Final integration tests must also exercise the public shared-library boundary.

## 27. Third-Party Dependency Policy

Third-party dependencies are isolated behind dedicated modules or integration
targets.

Initial expected dependencies are:

### Dear ImGui

- official repository;
- `docking` branch;
- exact pinned commit;
- viewer and `elf3d_imgui` only.

### GLFW

- official repository;
- exact pinned stable revision;
- viewer and ImGui platform integration only.

### GLM

- internal to `elf3d_math`;
- not visible in public DLL headers or plugin ABI.

### cgltf

- internal to `elf3d_gltf`;
- not visible outside the importer module.

### Image decoder

An initial PNG/JPEG decoder may be used inside an image or asset module.

Its types must not leak into Scene or the public API.

Dependencies must be pinned, documented, and licensed appropriately.

A dependency is not added merely because it may be useful later.

## 28. Testing Strategy

Testing uses complementary layers.

### Unit tests

Used for:

- math;
- transforms;
- bounds;
- handles;
- scene logic;
- navigation;
- picking algorithms;
- error and result types.

### Component tests

Used for:

- glTF import;
- scene construction;
- resource preparation;
- render-list generation;
- tool behavior.

### Integration tests

Used for:

- shared-library exports;
- public facade lifetime;
- graphics backend creation;
- off-screen rendering;
- viewer-to-engine integration;
- built-in module registration;
- runtime plugin loading when introduced.

### Render tests

Stable rendering cases may use:

- off-screen rendering;
- known test models;
- captured reference images;
- tolerant image comparison.

### Manual and visual testing

`elf3d_viewer` is used for:

- interactive navigation;
- docking behavior;
- visual inspection;
- performance statistics;
- tool interaction;
- regression reproduction.

The viewer is not a replacement for automated tests.

Automated tests are not a replacement for visual validation.

## 29. Initial Development Stages

### Stage 0: Project skeleton

Create:

- `elf3d_core`;
- the `elf3d` shared facade;
- `elf3d_imgui`;
- `elf3d_viewer`;
- CMake presets;
- Dear ImGui docking integration;
- GLFW and OpenGL GUI presentation;
- a minimal DLL lifetime test.

No scene or renderer is implemented.

### Stage 1: Math and graphics foundation

Add:

- `elf3d_math`;
- internal GLM integration;
- public Elf3D value types;
- basic graphics abstraction;
- initial OpenGL backend;
- `elf3d_viewport`;
- off-screen color and depth targets;
- native texture interoperation;
- viewport output displayed through Dear ImGui.

### Stage 2: Minimal scene and rendering

Add:

- `elf3d_assets`;
- `elf3d_scene`;
- `elf3d_renderer`;
- `Scene`;
- entity or node hierarchy;
- transforms;
- camera;
- mesh data;
- simple material;
- basic renderer.

### Stage 3: glTF vertical slice

Add:

- `elf3d_gltf`;
- cgltf integration;
- `.gltf` and `.glb` loading;
- scene bounds;
- automatic fit-to-view.

The vertical path becomes:

```text
GLB or glTF
    → Elf3D Scene
    → Viewport
    → Renderer
    → off-screen texture
    → ImGui::Image
```

### Stage 4: Navigation

Add:

- orbit or examine;
- pan;
- zoom;
- fit to scene;
- reset camera.

### Stage 5: Picking and selection

Add:

- viewport picking;
- stable entity identifiers;
- selection state;
- selection highlight;
- viewer demonstration.

### Stage 6: First tool

Add:

- measurement tool as a built-in engine module;
- overlay rendering;
- viewer controls.

### Stage 7: Section and clipping

Add:

- neutral clipping math and fixed-size filter data;
- one section plane and up to three axis-aligned clipping boxes per Viewport;
- shader clipping and Renderer broad-phase rejection;
- clipping-aware Picking, Selection, Measurement, visible bounds, fit, and reset;
- helper overlays and public viewer controls.

### Stage 8: Plugin experiment

After the tool contract is stable:

- define a versioned plugin ABI;
- convert or duplicate one small tool as a runtime DLL plugin;
- validate registration, lifetime, error handling, and shutdown.

## 30. Architectural Invariants

The following rules must always remain true:

1. The host application owns the operating-system event loop.
2. The host application owns the application main loop.
3. The host application owns Dear ImGui.
4. The engine core has no Dear ImGui dependency.
5. The engine does not create application menus or panels.
6. The viewer depends on the engine; the engine never depends on the viewer.
7. The viewer uses the public shared-library API.
8. Internal OBJECT-library and named-module boundaries are hidden from host
   applications.
9. Built-in modules are registered explicitly.
10. Major module dependencies remain acyclic.
11. Scene data does not depend on a concrete graphics backend.
12. Scene does not depend on Renderer.
13. Renderer does not own or silently mutate logical scene data.
14. Graphics API types remain inside graphics and backend boundaries.
15. GLM types do not cross the public DLL or plugin ABI boundary.
16. cgltf types remain inside the glTF importer.
17. Viewport input is supplied explicitly by the host.
18. Multiple viewports may display the same scene.
19. Shared scene access does not automatically imply shared ownership.
20. GPU resources are destroyed before their graphics device or context.
21. External data is validated before entering trusted engine structures.
22. Runtime plugins use a versioned narrow ABI.
23. Exceptions do not cross prohibited binary or thread boundaries.
24. The reference viewer remains a demonstration and test application, not the
    owner of engine functionality.
25. New modules are introduced for real responsibilities, not speculative
    architecture.

## 31. Architectural Decision Rule

When several designs satisfy the requirements, choose the design that:

1. keeps ownership and lifetime most explicit;
2. preserves the dependency direction;
3. exposes the smallest stable public interface;
4. avoids leaking third-party or backend types;
5. is easiest to test independently;
6. keeps necessary complexity local;
7. remains portable;
8. is easiest for developers and AI tools to understand and modify.

Elf3D should evolve as a modular engine, but modularity must remain a means of
controlling complexity rather than an end in itself.
