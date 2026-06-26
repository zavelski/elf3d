# AGENTS.md

## Purpose

This file gives Codex and other AI-assisted development tools the minimum
repository-level instructions required to work safely in Elf3D.

It does not replace the project documentation.

Before making changes, read the relevant parts of:

1. `CODING_POLICY.md`;
2. `ARCHITECTURE.md`;
3. this file;
4. any closer `AGENTS.md` in the directory being modified.

## Sources of Truth

Use the following precedence:

1. the explicit user task;
2. the nearest applicable `AGENTS.md`;
3. `ARCHITECTURE.md` for system boundaries and dependency direction;
4. `CODING_POLICY.md` for implementation and style rules;
5. existing code and tests.

Do not silently reinterpret an architectural rule to make a local change easier.

If a task genuinely requires an architectural exception, keep it local,
document the reason, and report it clearly.

## Git and GitHub Workflow Routing

Ordinary requests to create a commit on GitHub, publish current work, commit and
push, or synchronize a completed ordinary change invoke `$elf3d-publish-change`.
Ordinary publication means validation, documentation review, logical commits,
push, and CI verification; it never implies a version release.

Explicit named-version release requests invoke `$elf3d-release`. Ordinary
development must not be committed directly to `main`. Release completion
requires remote `main` and `develop` to have identical head commits. Release
tags point to the validated release source commit and are never moved for later
documentation. Post-release documentation must be synchronized into both
branches.

Never force-push and never move a published tag. Stop on secrets, private
models, remote conflicts, failing tests, or authentication issues. Living
documentation changes together with implementation. Historical release
snapshots remain immutable except for explicitly designated post-release
reports.

## Project Model

Elf3D is a portable C++20 3D visualization engine.

The primary product is one public shared library:

```text
elf3d.dll
```

The shared library is assembled from internal CMake OBJECT-library modules.
C++20 named modules define the current internal engine logical boundaries.

The repository also contains:

```text
elf3d_imgui
    Optional Dear ImGui host-integration library.

elf3d_viewer
    Reference application, demonstration program, and graphical testbed.
```

The host application owns:

- the native window;
- the operating-system event loop;
- the application main loop;
- Dear ImGui;
- GUI construction;
- final frame presentation.

The engine must not create or own those application-level facilities.

## Required Dependency Direction

Maintain these rules:

- `elf3d_viewer` may depend on `elf3d_imgui` and the public `elf3d` API.
- `elf3d_imgui` may depend on Dear ImGui, GLFW, OpenGL presentation code, and
  the public `elf3d` API.
- `elf3d` must not depend on `elf3d_imgui` or `elf3d_viewer`.
- The engine core must not depend on Dear ImGui, GLFW, or application GUI code.
- Scene code must not depend on Renderer.
- Scene and Assets must not depend on glTF-specific types.
- Renderer may consume Scene and Asset data but must not own or silently mutate
  the logical scene.
- Native graphics API types must remain inside graphics backend boundaries.
- Navigation, Picking, and Tools must not query Dear ImGui directly.
- Major module dependencies must remain acyclic.

Do not introduce a reverse dependency to avoid defining a proper lower-level
interface.

## Public API Rules

The public C++ API uses:

```cpp
namespace elf3d {
}
```

Use ordinary public type names such as:

```cpp
elf3d::Engine
elf3d::Scene
elf3d::Viewport
```

Use:

```text
ELF3D_
```

for global macros and build definitions.

Do not expose these types through public Elf3D headers:

- Dear ImGui types;
- GLFW types;
- OpenGL or other native graphics API types;
- GLM types;
- cgltf types;
- private module classes;
- internal containers or caches.

Keep ownership and destruction across the DLL boundary explicit.

Objects created by Elf3D must be destroyed through supported Elf3D APIs or
exported facade destructors.

## Module Rules

Create a new module or CMake target only when it has real functionality and a
clear architectural responsibility.

Do not create empty placeholder modules for possible future features.

Internal engine modules are normally CMake OBJECT libraries linked into
`elf3d`. New engine components should prefer a meaningful C++20 named module
interface plus implementation units instead of a new internal static library.

Do not create a new internal static library unless packaging, reuse outside
`elf3d`, tooling, external integration, or another concrete reason makes an
OBJECT library unsuitable.

Do not convert a module into a separate DLL unless independent deployment,
runtime selection, licensing, or replacement provides a concrete benefit.

C++ named-module export and DLL symbol export are separate mechanisms. Keep the
external DLL ABI narrow and explicit through supported export/import macros such
as `ELF3D_API`. Do not expose third-party types through exported project module
interfaces, and do not treat BMI, IFC, PCM, or GCM files as distributable
artifacts.

For the current named-module migration:

- use meaningful dotted names such as `elf.core`, `elf.math`, `elf.assets`,
  and `elf.render`;
- dotted names are naming conventions, not language-level nesting;
- do not use module partitions, private module fragments, header units, or
  mandatory `import std`;
- keep ordinary standard-library `#include` use;
- do not modularize third-party dependencies;
- avoid import cycles and excessive one-class module granularity.

Built-in modules must be registered explicitly by the composition root.

Do not use hidden self-registration through global constructors.

Runtime plugins, when introduced, must use the versioned plugin ABI defined by
the architecture.

## Third-Party Dependencies

Do not add a dependency without a concrete requirement.

Every dependency must be:

- obtained from its official source;
- pinned to an exact reproducible revision;
- isolated behind the appropriate module or integration target;
- recorded in `THIRD_PARTY.md`;
- accompanied by required license notices.

Project-specific rules:

- Dear ImGui uses the official `docking` branch pinned to an exact commit.
- Dear ImGui is allowed only in the viewer and ImGui integration layer.
- GLFW is allowed only in host/platform integration code.
- GLM may be used internally by `elf3d_math` but must not enter the public ABI.
- cgltf may be used internally by `elf3d_gltf` but must not leak from that
  module.

Do not edit third-party source code unless the task explicitly requires a
reviewed local patch.

## Build System

CMake is the authoritative build system.

Do not create or maintain a handwritten Visual Studio solution.

Use target-based CMake:

- target include directories;
- target compile definitions;
- target compile options;
- target link dependencies.

Avoid global include directories, compiler definitions, link directories, and
warning settings when a target-scoped alternative exists.

Use ISO C++20 with compiler extensions disabled.

Before building, inspect:

```text
CMakePresets.json
```

and use the existing project presets.

Typical commands are:

```bash
cmake --preset <debug-preset>
cmake --build --preset <debug-build-preset>
ctest --preset <debug-test-preset>
```

Do not invent new preset names when suitable presets already exist.

When no preset exists for the required configuration, add one only when the task
requires it and keep naming consistent with the existing file.

On MSVC, use the dynamic runtime consistently:

```text
Debug:   /MDd
Release: /MD
```

Do not apply project warning-as-error settings to third-party code.

## Current Development Principle

Implement the smallest complete vertical slice required by the task.

Do not add speculative infrastructure.

In particular, do not introduce these without a current concrete requirement:

- an ECS;
- a global event bus;
- a service locator;
- a universal plugin framework;
- a task graph;
- a custom memory manager;
- a renderer framework with unused abstraction layers;
- multiple graphics backends before the first backend works;
- generalized editors or GUI frameworks.

Prefer one working path over several unfinished abstractions.

## Viewer Rules

`elf3d_viewer` is a reference application and graphical testbed.

It may grow as engine functionality grows, but it must remain focused on:

- demonstrating engine capabilities;
- manual and visual validation;
- reproducing defects;
- reporting useful diagnostics and statistics.

The viewer must use the public Elf3D API.

It must not include private engine headers or link directly to internal engine
OBJECT modules.

GUI code belongs in the viewer or `elf3d_imgui`, not in the engine.

Dear ImGui docking is enabled in the viewer.

Dear ImGui multi-viewport support is not enabled unless a separate task adds and
validates it.

## Ownership and Lifetime

Follow the ownership model in `ARCHITECTURE.md`.

Important defaults:

- the host owns `Engine`;
- the application or document layer normally owns `Scene`;
- a `Viewport` observes a scene and does not imply shared ownership;
- the scene owns logical scene data;
- the renderer owns render-specific GPU representations;
- GPU resources must be destroyed before their graphics device or context;
- the engine must be destroyed before required host graphics infrastructure.

Do not introduce `std::shared_ptr` merely because several systems access the
same object.

## Error Handling

Follow `CODING_POLICY.md`.

In particular:

- assertions are for programming errors;
- expected failures use the project `Result<T>` mechanism;
- normal absence uses `std::optional<T>` where appropriate;
- exceptions must not cross C ABI, plugin ABI, thread-entry, or prohibited
  callback boundaries;
- low-level modules do not display GUI errors;
- the host decides how errors are presented to the user.

Do not log and propagate the same error repeatedly at every layer.

## Testing and Validation

A change is not complete merely because it compiles.

Use the validation appropriate to the change:

- unit tests for math, value types, algorithms, and scene logic;
- component tests for importers, navigation, picking, and rendering preparation;
- integration tests for the public DLL boundary, graphics backend, and viewer;
- manual viewer validation for rendering and interaction;
- benchmarks for performance-motivated complexity.

For every task:

1. build all affected targets;
2. run relevant tests;
3. inspect warnings;
4. review the final diff;
5. verify that no unrelated files changed;
6. report exactly what was and was not validated.

Do not claim that GUI or rendering behavior was verified when the viewer could
not be launched.

## Working Style

Before editing:

1. inspect the repository structure;
2. read the nearest relevant documentation;
3. locate the real ownership and dependency boundaries;
4. identify existing tests and build targets.

During editing:

- keep changes focused;
- preserve public contracts unless the task changes them;
- avoid unrelated cleanup;
- prefer existing project mechanisms;
- do not introduce duplicate result, logging, handle, event, or ownership
  abstractions;
- keep implementation details private;
- add comments only for non-obvious reasons, invariants, or constraints.

After editing:

- format only relevant project-owned files;
- build and test;
- inspect the complete diff;
- update documentation only when behavior or architecture actually changed.

## Documentation Changes

Do not rewrite `CODING_POLICY.md` or `ARCHITECTURE.md` as part of an unrelated
implementation task.

Update `ARCHITECTURE.md` when a deliberate architectural decision changes:

- module responsibility;
- dependency direction;
- ownership;
- public binary boundary;
- plugin model;
- threading model;
- major data flow.

Update `README.md` for user-visible build, run, or feature changes.

Update `THIRD_PARTY.md` whenever a dependency or pinned revision changes.

## Documentation Maintenance Rules

Documentation is part of the implementation. When a task changes behavior,
interfaces, validation, or release status, update the corresponding
documentation in the same task.

Review documentation whenever a task affects:

- public API;
- exported symbols;
- CMake targets;
- module dependencies;
- glTF support;
- render passes;
- shaders;
- color space;
- GPU caches;
- Viewport input;
- navigation;
- Picking;
- Selection;
- visibility;
- isolation;
- Measurement;
- clipping;
- lifetime;
- threading;
- tests;
- fixtures;
- CI;
- viewer behavior;
- performance;
- roadmap status.

Use `docs/DOCUMENTATION_POLICY.md` and
`docs/DOCUMENTATION_UPDATE_CHECKLIST.md` for review procedure.

Code changes and corresponding documentation changes belong to the same task
unless the user explicitly requests otherwise. Planned features must not be
documented as implemented. Current technical documents must be checked against
actual code, tests, and validation results before being updated. Released
project-state snapshots must not be silently rewritten; create a new release
snapshot or living-document update instead.

## Completion Report

At the end of a task, report:

- the implementation completed;
- the important files or targets changed;
- any public API changes;
- any architecture or dependency changes;
- dependency revisions added or changed;
- exact configure, build, and test commands executed;
- whether `elf3d_viewer` was launched;
- what was manually verified;
- what could not be verified;
- remaining limitations or follow-up work.

Keep the report factual.

Do not describe planned work as completed.
