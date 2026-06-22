# C++ Coding Policy

## Purpose

This project uses ISO C++ 20 and the C++ standard library within a deliberately
simple, explicit, portable, and widely understood subset of the language.

Simplicity is a design constraint, not merely a stylistic preference. Code must
be readily understandable to ordinary professional C++ developers, including
those who are not specialists in template metaprogramming, compiler internals,
or obscure language rules. It must also be easy for AI-assisted development
tools to parse, reason about, modify, and validate reliably.

The goal is not to avoid powerful C++ features. The goal is to use as many
powerful, safe, and efficient modern C++ features as possible while keeping
ownership, lifetime, control flow, failure, side effects, and invariants locally
understandable.

## Policy levels

- **Required** — mandatory project rule. A deviation requires an explicit policy
  exception.
- **Preferred** — normal first choice. Another solution is acceptable when it is
  clearly better for the concrete requirement.
- **Restricted** — allowed only for a concrete need, with complexity isolated
  and appropriately validated.

Explicit words such as `must`, `should`, `prefer`, `avoid`, and `prohibited` are
authoritative.

## Operational summary

1. Use the most powerful standard C++ solution that remains ordinary, explicit,
   portable, and easy to reason about.
2. Prefer local reasoning: ownership, lifetime, mutation, failure, allocation,
   blocking, and control flow should be visible from nearby code.
3. Prefer values, RAII, direct ownership, standard containers, and standard
   algorithms.
4. Use `std::unique_ptr` for exclusive dynamic ownership. Use
   `std::shared_ptr` only for genuine shared lifetime. Raw pointers are
   non-owning.
5. Prefer concrete code and composition. Introduce inheritance, templates,
   callbacks, concurrency, or custom infrastructure only when they reduce total
   complexity or meet a concrete requirement.
6. Use explicit result types for expected runtime failures. Use exceptions only
   for uncommon failures handled at a higher architectural boundary. Use
   assertions for programming errors.
7. Do not hide important behavior behind macros, implicit conversions, long
   callback chains, deep inheritance, or advanced template machinery.
8. Use the standard library by default. `std::vector` is the default dynamic
   sequence container.
9. Optimize only against real requirements. Measure before and after adding
   non-trivial complexity.
10. Keep platform-specific, ABI-sensitive, low-level, and performance-specific
    code behind small portable interfaces.
11. Compile cleanly with all supported toolchains and validate changes according
    to their risk.
12. When several solutions are correct, choose the one safest for humans and AI
    tools to understand and modify.

## Table of Contents

1. [Core Principles](#1-core-principles)
2. [Language Standard and Portability](#2-language-standard-and-portability)
3. [Code Structure and Readability](#3-code-structure-and-readability)
4. [Types, Variables, Initialization, and `auto`](#4-types-variables-initialization-and-auto)
5. [Functions and Interfaces](#5-functions-and-interfaces)
6. [Ownership, Lifetime, and RAII](#6-ownership-lifetime-and-raii)
7. [Error Handling](#7-error-handling)
8. [Classes and Object Design](#8-classes-and-object-design)
9. [Inheritance and Runtime Polymorphism](#9-inheritance-and-runtime-polymorphism)
10. [Templates and Generic Programming](#10-templates-and-generic-programming)
11. [Standard Library, Containers, Algorithms, and Strings](#11-standard-library-containers-algorithms-and-strings)
12. [Lambdas, Callbacks, and Events](#12-lambdas-callbacks-and-events)
13. [Operators, Conversions, and Comparison](#13-operators-conversions-and-comparison)
14. [Compile-Time Programming and the Preprocessor](#14-compile-time-programming-and-the-preprocessor)
15. [Concurrency and Asynchronous Work](#15-concurrency-and-asynchronous-work)
16. [Performance and Memory](#16-performance-and-memory)
17. [Modules, Dependencies, and Boundaries](#17-modules-dependencies-and-boundaries)
18. [Naming, Formatting, Comments, and Documentation](#18-naming-formatting-comments-and-documentation)
19. [Testing, Analysis, and Review](#19-testing-analysis-and-review)
20. [Policy Enforcement and Final Checklist](#20-policy-enforcement-and-final-checklist)

---

## 1. Core Principles

### Required

- Code must favor local reasoning. A reader should normally be able to determine
  what a function or type does, what it owns, what it modifies, what can fail,
  and which invariants it preserves without tracing a large part of the codebase.
- Important behavior must not be hidden behind distant conventions, implicit
  conversions, macro control flow, long callback chains, deep inheritance, or
  complex template machinery.
- Correctness, resource safety, and required performance must not be sacrificed
  for superficial simplicity.
- Necessary complexity must be isolated behind a small, ordinary interface.
- Do not design for hypothetical future requirements when a simpler design fully
  satisfies current requirements and remains reasonably extensible.

### Preferred

- Use RAII, value semantics, move semantics, standard containers and algorithms,
  scoped enumerations, strong types, typed time values, and explicit ownership.
- Create abstractions around stable domain concepts, ownership, invariants, or
  repeated behavior that changes for the same reason.
- Prefer a small amount of obvious duplication over a premature abstraction that
  couples unrelated concepts.

### Restricted

The following require concrete justification:

- advanced template metaprogramming;
- subtle overload resolution or lifetime rules;
- expression-template frameworks;
- deep abstraction layers;
- hidden mutable global state;
- macro-based control flow;
- custom language-like frameworks.

## 2. Language Standard and Portability

### Required

- Production code, tests, tools, and examples use ISO C++20.
- Use strict standard modes:
  - MSVC: `/std:c++20`;
  - GCC: `-std=c++20`;
  - Clang: `-std=c++20`.
- Do not use preview or newer-language modes in production without an explicit
  project-wide decision.
- Maintain a documented support matrix containing minimum compiler,
  standard-library, operating-system, and architecture versions.
- Required behavior must compile and work across supported MSVC, GCC, and Clang
  toolchains.
- Do not rely on GNU, Microsoft, or Clang language extensions in portable code.
- Use fixed-width integer types when representation is part of an ABI, file
  format, protocol, hardware interface, or serialization contract.
- Do not serialize native object layouts directly. Define sizes, byte order,
  alignment, padding, and versioning explicitly.
- Portable code must not depend on signed overflow, strict-aliasing violations,
  invalid lifetime, unaligned access, unspecified evaluation order, or other
  undefined behavior.

### Preferred

- Use standard feature-test macros when conditional compilation is necessary.
- Keep compiler and platform differences in dedicated portability files or
  implementation layers.
- Compile important targets regularly with every supported compiler family.

### Restricted

The following are not part of the default vocabulary and require proven support
across the toolchain matrix or an isolated fallback:

- C++ language modules;
- coroutines;
- parallel standard algorithms;
- complex range-view pipelines;
- compiler intrinsics and native vector types;
- facilities with incomplete or inconsistent library support.

## 3. Code Structure and Readability

### Required

- Optimize code for correct understanding, not minimum line count.
- Each function should perform one coherent operation at one level of
  abstraction.
- Keep the successful main path easy to identify.
- Prefer early returns for invalid input, failed preconditions, and edge cases
  when they reduce nesting.
- Use ordinary `if`, `switch`, `for`, and `while` when they express control flow
  most clearly.
- Avoid deep nesting, side effects inside conditions, and statements that combine
  unrelated acquisition, validation, mutation, and dispatch.
- Introduce named intermediate values when they clarify concepts, units,
  validation, or debugging.
- Source files and modules must have narrow, coherent responsibilities.

### Preferred

- Extract a helper when it names a meaningful operation, isolates non-obvious
  logic, or removes difficult nesting.
- Keep simple sequential logic together rather than splitting it into many tiny
  functions.
- Use domain-specific names instead of generic `data`, `item`, `helper`, or
  `manager` when a more precise term exists.
- Keep implementation at one abstraction level within a function.

### Restricted

- Clever code, puzzle-like expressions, or techniques requiring obscure
  language knowledge are prohibited unless they provide a substantial,
  demonstrated benefit.
- Recursion should be used only when it directly matches the problem and has a
  clearly bounded depth.

## 4. Types, Variables, Initialization, and `auto`

### Required

- Types should communicate important meaning, ownership, units, identity, valid
  states, and constraints.
- Initialize every object before use, preferably at declaration.
- Construct valid objects in one operation where practical.
- Use `nullptr`, never `0` or `NULL`, for null pointers.
- Avoid arbitrary sentinel values when absence or invalidity is part of the
  domain; represent it explicitly.
- Keep variable scope narrow and do not reuse one variable for different
  conceptual values.
- Make narrowing and representation-changing conversions explicit and validate
  their range.
- Avoid C-style casts.

### Preferred

- Use `enum class` for closed sets of states.
- Use `std::size_t` for standard-container sizes and indices, except where an
  external representation requires another type.
- Prefer immutable local variables and meaningful `const` qualification.
- Use `auto` when the initializer makes the type obvious or spelling the type
  adds noise.
- Use an explicit type when it communicates ownership, precision, signedness,
  units, domain meaning, or intentional conversion.
- Use simple aggregate `struct` types for transparent groups of data.
- Use `using` instead of `typedef` in new code.

### Restricted

- Avoid `auto` when it hides ownership or an important semantic type.
- Avoid `auto&&` and `decltype(auto)` in ordinary non-generic code.
- Use `reinterpret_cast` and `const_cast` only at isolated low-level or external
  boundaries with documented assumptions.
- Do not introduce strong-wrapper types when they create more conversion noise
  than safety.

## 5. Functions and Interfaces

### Required

A function declaration should make clear:

- inputs and outputs;
- mutation;
- optionality;
- ownership transfer;
- expected failure behavior.

Additional rules:

- Do not hide important outputs in undocumented side effects.
- Return references or pointers only when the referenced lifetime is clear.
- Do not return references or views into temporaries or short-lived local data.
- Use non-const references only for intentional, visible mutation.
- Public interfaces must not use ambiguous positional booleans or several
  similar primitive arguments when a named type or options object is clearer.
- Mark member functions `const` when they do not change observable state.

### Preferred

Default parameter forms:

- small inexpensive values: `T`;
- mandatory read-only object: `const T&`;
- mandatory mutable object: `T&`;
- optional non-owning object: `T*` or `const T*`;
- contiguous sequence view: `std::span<T>`;
- read-only non-retained text: `std::string_view`.

Further defaults:

- Prefer returning values over output parameters.
- Prefer a named result `struct` over a multi-field `std::tuple`.
- Pass a sink parameter by value and move it into storage when that yields one
  simple efficient interface.
- Use free functions for algorithms that do not require object invariants or
  privileged access.
- Use `[[nodiscard]]` when ignoring a result is likely to be a bug.
- Use `noexcept` when the guarantee is true and semantically useful, especially
  for destructors, move operations, swaps, and cleanup.

### Restricted

- Output parameters are allowed for storage reuse, append operations, external
  APIs, or measured allocation-sensitive paths.
- Overloading is allowed only when overloads represent the same conceptual
  operation and cannot be confused by ordinary conversions.
- Default arguments must represent stable, unsurprising defaults.
- Do not use smart-pointer parameters merely because the caller stores the
  object in a smart pointer. Use them only when ownership participation is part
  of the contract.

## 6. Ownership, Lifetime, and RAII

### Required

- Every resource must have a clear owner and deterministic cleanup.
- Use RAII for memory, files, sockets, handles, locks, mappings, graphics
  resources, transactions, and other paired acquire/release operations.
- Raw pointers are non-owning in normal project code.
- A reference expresses mandatory non-owning access; a pointer expresses
  optional or reseatable non-owning access.
- Non-owning views, references, pointers, spans, iterators, and string views must
  not outlive their owner.
- Do not retain views across container operations that may invalidate them.
- Copy and move behavior must match the semantic meaning of the type.
- Destructors must not throw.
- Ownership graphs must have a clear direction and must not contain accidental
  shared-pointer cycles.

### Preferred

- Prefer values and direct members over dynamic allocation.
- Use `std::unique_ptr<T>` as the default representation of exclusive dynamic
  ownership.
- Use `std::make_unique` and `std::make_shared` rather than direct `new`.
- Prefer the Rule of Zero. Let standard RAII members manage resources.
- Resource-owning types should normally be movable; copying should exist only
  when it has clear value semantics.
- Wrap external handles in small movable, usually non-copyable RAII types.

### Restricted

- Use `std::shared_ptr<T>` only when several independent owners genuinely must
  keep the same object alive. Shared access alone is not shared ownership.
- Use `std::weak_ptr<T>` only to observe shared ownership without extending it
  or to break cycles.
- Stored raw pointers or references require a simple, explicit owner-outlives-
  observer guarantee.
- Pimpl is allowed for ABI control, large dependency isolation, or platform
  separation, not as a default class pattern.
- Custom deleters and scope guards should remain small and should not hide
  business logic.
- Avoid mutable owning global objects and implicit static-lifetime ownership.

## 7. Error Handling

### Required

Distinguish these categories:

1. programming errors and violated invariants;
2. normal absence;
3. expected runtime failures;
4. exceptional failures handled at a higher level;
5. unrecoverable process-level failures.

Use them consistently:

- Assertions are for programming errors, never for untrusted input or normal
  runtime failure.
- `std::optional<T>` represents normal absence, not detailed failure.
- The project must use one small, consistent `Result<T>`-style abstraction for
  expected failures in C++20.
- A single operation must not report the same expected failure sometimes through
  `Result<T>` and sometimes through exceptions.
- Exceptions must not cross C ABI, plugin ABI, foreign callback, or thread-entry
  boundaries.
- Destructors must not throw.
- External input must be validated for sizes, counts, offsets, indices, ranges,
  enum values, arithmetic overflow, and resource limits.
- Failure must never be silently treated as success.

### Preferred

- Propagate expected errors explicitly when the current layer cannot handle
  them.
- Add context at architectural boundaries without repeatedly wrapping the same
  message.
- Use structured error categories or codes plus human-readable context.
- Use a fallible factory returning `Result<T>` when construction failure is
  expected and needs direct handling.
- Catch exceptions only to recover, add context, translate mechanisms, or enforce
  an architectural boundary.
- Log errors at the layer that decides their application-level consequence.

### Restricted

- Exceptions are allowed for uncommon failures whose useful handler is several
  layers higher and where explicit propagation would obscure normal logic.
- Do not use exceptions for search miss, validation failure, malformed ordinary
  input, loop termination, or common file absence.
- `catch (...)` is allowed only at a final boundary that must prevent escape.
- Error-propagation macros are allowed only as one established project mechanism
  with obvious early-return behavior and predictable scope.
- Process termination is reserved for corrupted state, escaped final-boundary
  exceptions, or failures after which continuation is unsafe or meaningless.

## 8. Classes and Object Design

### Required

- Every type should represent one coherent concept.
- Successful construction should establish a valid object and its invariants.
- Public operations must preserve those invariants.
- Keep public interfaces small and meaningful.
- Do not expose mutable internal state when arbitrary mutation can invalidate the
  object.
- Dependencies and their lifetimes must be explicit.
- Copying, moving, identity, and equality must match the domain meaning.

### Preferred

- Use `struct` for transparent data aggregates and `class` for resource
  management, hidden representation, controlled mutation, or non-trivial
  invariants.
- Prefer composition over inheritance.
- Prefer domain operations over sequences of low-level setters.
- Prefer data-oriented types and free algorithms when the problem is naturally
  batch-oriented or transformation-oriented.
- Use explicit single-argument constructors by default.
- Prefer concrete classes when only one implementation is required.

### Restricted

- Builders are justified only for genuinely complex, staged, or heavily
  optional construction.
- Factories are justified for fallible construction, named variants, runtime
  implementation selection, or platform isolation.
- `friend` should be rare and must not compensate for a poor interface.
- Avoid classes that are merely namespaces for static functions.
- Be skeptical of broad `Manager`, `Context`, `Utility`, or `Service` classes
  with unrelated responsibilities.
- Avoid two-phase initialization unless the domain genuinely requires explicit
  states.

## 9. Inheritance and Runtime Polymorphism

### Required

- Public inheritance must represent real substitutability.
- A derived type must preserve the behavioral contract of its base type.
- Polymorphic bases that can be destroyed through base pointers must have a
  virtual destructor.
- Every overriding function must use `override`.
- Keep interfaces small, stable, and ownership-explicit.
- Use `std::unique_ptr<Base>` as the default owning representation for a
  polymorphic object.

### Preferred

- Prefer a concrete class when one implementation exists and runtime
  substitution is not required.
- Use `std::variant` for a small closed set of alternatives.
- Use a virtual interface for an open set of independently extensible runtime
  implementations.
- Prefer strategy composition when one behavior varies independently from the
  main object.
- Mark concrete implementation classes `final` when they are not extension
  points.

### Restricted

- Do not use inheritance merely for implementation reuse.
- Avoid protected data, deep hierarchies, private inheritance, and virtual
  inheritance.
- Multiple inheritance is limited primarily to several small independent pure
  interfaces.
- RTTI, `dynamic_cast`, and downcasting must not form normal core control flow.
- Do not call overridable virtual functions from constructors or destructors.
- Visitors, Template Method, NVI, cloning, and mixins require a concrete need and
  must remain small.

## 10. Templates and Generic Programming

### Required

- Genericity must be justified by real uses or by an essential reusable
  abstraction.
- Template code must remain understandable to a general professional C++
  developer.
- Keep template parameter counts, constraint expressions, and instantiation
  paths small.
- Generic code must not hide ownership, control flow, allocation, or failure.
- Compiler diagnostics for ordinary misuse should remain understandable.
- Public template definitions and their dependency cost must be managed
  deliberately.

### Preferred

- Start with concrete code when the abstraction is not yet clear; generalize
  after real repeated use reveals a stable common operation.
- Use C++20 concepts to express short, meaningful requirements.
- Prefer standard concepts and direct `requires` expressions.
- Use simple function and class templates for algorithms or containers that
  naturally vary by type.
- Use `static_assert` for important compile-time invariants with actionable
  messages.

### Restricted

The following require strong justification and isolation:

- partial specialization and large custom trait systems;
- SFINAE in new C++20 code;
- recursive template metaprogramming;
- variadic interfaces beyond simple folds or forwarding;
- perfect forwarding outside generic infrastructure;
- unconstrained forwarding constructors;
- policy-based design and CRTP;
- expression templates;
- custom type erasure;
- large compile-time computations.

When two or three concrete implementations are easier to understand than one
framework, prefer the concrete implementations.

## 11. Standard Library, Containers, Algorithms, and Strings

### Required

- Use the standard library as the default foundation.
- Do not reimplement a standard facility without a concrete need.
- Understand and respect container invalidation, ownership, iterator, and
  reference rules.
- Do not depend on unordered-container iteration order.
- Do not depend on standard-library implementation details.
- Define the project text encoding; UTF-8 is preferred for portable internal
  text unless an external API requires another encoding.
- Do not pass `string_view.data()` to an API requiring null termination unless
  termination is guaranteed.

### Preferred

- `std::vector<T>` is the default dynamically sized sequence.
- Use `std::array<T, N>` for fixed-size value sequences.
- Prefer contiguous value containers for locality and simple ownership.
- Use `std::span` for non-owning contiguous views.
- Use range-based `for` for straightforward traversal.
- Use standard algorithms when their names make intent clearer than a loop.
- Use an ordinary loop when state, early exit, error handling, or multiple steps
  are central.
- Use `std::string` for owning text and `std::string_view` for non-owning,
  non-retained read-only text.
- Use `std::filesystem::path` for filesystem paths.
- Use ordered or explicitly sorted data when deterministic output matters.

### Restricted

- `std::list` and `std::forward_list` require a real need for splicing, stable
  addresses, or iterator-based insertion/removal.
- Long lazy range pipelines are discouraged; materialize results when ownership,
  repeated traversal, debugging, or lifetime would otherwise be unclear.
- `std::any`, large tuples, and broad type-erased interfaces are not default
  domain representations.
- Custom containers and allocators require a measured or representational need
  and documented ownership, invalidation, complexity, and thread-safety rules.
- `std::format` may be used only if supported consistently across the toolchain
  matrix; otherwise use one approved formatting solution.

## 12. Lambdas, Callbacks, and Events

### Required

A callback interface must define:

- whether invocation is immediate, deferred, or stored;
- synchronous or asynchronous execution;
- execution thread;
- ownership and lifetime;
- copy or move behavior;
- one-shot or repeated invocation;
- exception and cancellation behavior.

Additional rules:

- Captured references must outlive every invocation.
- Do not capture stack references or `this` in asynchronous work unless lifetime
  is explicitly guaranteed.
- Do not invoke unknown external callbacks while object invariants are broken or
  while holding a mutex unless the contract explicitly requires it.
- Subscription lifetime must be explicit; prefer RAII disconnection.

### Preferred

- Use short local lambdas for predicates, transformations, comparators, and
  immediate adapters.
- Capture only required values; prefer explicit captures for stored or escaping
  lambdas.
- Use a function pointer for a stateless callback with a fixed signature.
- Use `std::function` when stored runtime type erasure is part of the interface.
- Use a named function object when a callable has substantial state or behavior.
- Prefer one result callback or result object over separate loosely coordinated
  success and error callbacks.

### Restricted

- Extract a named function when a lambda becomes long, reusable, branch-heavy,
  or domain-significant.
- Event systems are justified only when producers and multiple independent
  consumers genuinely need decoupling.
- Avoid global event buses, stringly typed events, untyped payloads, and hidden
  cross-module control flow.
- `std::function` should be avoided in measured hot loops when indirect dispatch
  or allocation matters.

## 13. Operators, Conversions, and Comparison

### Required

- An overloaded operator must have a natural, widely understood meaning for the
  type.
- Non-assignment arithmetic operators should not mutate operands.
- Compound assignment should mutate the left operand and return `*this`.
- Equality must have a stable, context-independent meaning.
- Ordering comparators must satisfy their required mathematical contract.
- Hashing must be consistent with equality.
- Narrowing and unit-changing conversions must be explicit.

### Preferred

- Use operators primarily for value-like mathematical, iterator, handle, or
  strongly typed numeric objects.
- Implement binary arithmetic through compound assignment where that keeps one
  mutation path.
- Use defaulted equality or three-way comparison only when member-wise behavior
  exactly matches domain semantics.
- Use named conversion functions for strings, units, native handles, and other
  semantically significant transformations.

### Restricted

- Single-argument constructors and conversion operators are `explicit` by
  default.
- Prefer named functions when an operation is expensive, fallible, mutating,
  allocates, performs I/O, or has several plausible meanings.
- Do not overload `&&`, `||`, comma, address-of, or class-specific `new/delete`
  in ordinary application code.
- Avoid approximate floating-point `operator==`; use a named comparison with an
  explicit tolerance.
- Avoid operator syntax that hides ownership transfer or large work.

## 14. Compile-Time Programming and the Preprocessor

### Required

- Compile-time programming must provide a concrete benefit in correctness,
  representation, or measured runtime performance.
- `constexpr` code should remain readable as ordinary C++.
- Macros must not evaluate arguments more than once or create surprising scope.
- Required behavior must not exist only inside assertions or debug-only blocks.
- Headers must be self-contained and include what they directly use.
- Generated code must be reproducible, deterministic, marked as generated, and
  ordinary enough to review and analyze.

### Preferred

- Use `constexpr` for inherent constants and simple functions usable at compile
  time and runtime.
- Use `consteval` only when runtime evaluation would violate an invariant.
- Use `constinit` when static initialization order must be controlled.
- Use `if constexpr` for short type-dependent branches.
- Prefer typed constants, inline functions, templates, attributes, and enums to
  macros.
- Use standard include guards for maximum portability.
- Keep corresponding headers first in source files and use a consistent include
  order.

### Restricted

- The preprocessor is limited to include guards, configuration, platform and
  compiler detection, symbol visibility, narrow diagnostics, assertions, and
  other tasks the language cannot express clearly.
- Function-like macros, X-macros, generated registration, and propagation macros
  require one clear project-wide convention and must remain small.
- Conditional compilation must be local and shallow; prefer separate platform
  source files for substantial differences.
- Avoid large compile-time tables, recursive metaprograms, and compile-time
  string systems when build-time tools or clear runtime code are simpler.

## 15. Concurrency and Asynchronous Work

### Required

- Introduce concurrency only for a concrete responsiveness, throughput, latency,
  or platform requirement.
- Every thread, task, and asynchronous operation must have a clear owner.
- Ownership must define startup, cancellation, completion, joining, exception
  handling, and shutdown.
- Detached threads are prohibited in normal project code.
- Minimize shared mutable state.
- Every shared mutable object must have an explicit synchronization contract.
- Do not use `volatile` for synchronization.
- Exceptions must not escape thread-entry or task boundaries.
- Thread affinity must be explicit for UI, rendering, platform, or other
  thread-confined objects.
- Concurrent subsystem shutdown must stop new work, request cancellation, wake
  waiters, complete or cancel work according to policy, join workers, and only
  then release dependencies.

### Preferred

- Prefer sequential code when it satisfies requirements.
- Prefer `std::jthread` and `std::stop_token` for owned background threads.
- Prefer immutable data, ownership transfer, partitioning, task-local state, and
  message passing over shared mutation.
- Use `std::mutex` with RAII lock types as the default synchronization mechanism.
- Keep lock scope limited and protect one clear invariant.
- Use condition variables with predicates.
- Use explicit bounded executors or thread pools when repeated task scheduling is
  required.

### Restricted

- `std::shared_mutex` requires measured read-heavy contention.
- Atomics are for small independent state or specialized infrastructure, not a
  replacement for clear locking.
- Use sequentially consistent atomic ordering by default. Weaker memory ordering
  requires a documented proof and specialist review.
- Avoid recursive mutexes, busy waiting, custom lock-free structures,
  thread-local hidden dependencies, and coroutine frameworks.
- Parallel algorithms and reductions require explicit treatment of ordering,
  cancellation, failure, and floating-point determinism.

## 16. Performance and Memory

### Required

- Performance complexity must be justified by an actual requirement.
- Measure before and after non-trivial optimization using representative data,
  optimized builds, and relevant target platforms.
- Preserve correctness, lifetime, error handling, portability, thread safety,
  and numerical requirements.
- Do not depend on undefined behavior or incidental compiler/library details.
- Keep specialized optimization behind a simple portable interface.

### Preferred

Optimize in this order:

1. remove unnecessary work;
2. choose a better algorithm;
3. choose a better data structure;
4. improve locality and access order;
5. reduce allocation and copying;
6. reduce synchronization and I/O overhead;
7. optimize hot loops;
8. use platform-specific instructions only if still required.

Additional defaults:

- Prefer contiguous storage and predictable traversal.
- Use `reserve()` when size is known well enough to matter.
- Reuse storage in repeated hot operations when ownership remains simple.
- Prefer value returns and rely on copy elision and moves.
- Keep hot loops simple and move invariant work outside them.
- Let the compiler auto-vectorize straightforward loops before using intrinsics.
- Use batching when fixed per-operation overhead is significant.

### Restricted

- Custom allocators, pools, and arenas require a measured allocation problem or
  a specific representation/lifetime requirement.
- Structure-of-arrays, hot/cold splitting, explicit alignment, cache-line
  padding, and false-sharing mitigation require workload evidence.
- Intrinsics, branchless techniques, forced inlining, manual unrolling, and
  platform dispatch require benchmarks.
- Fast-math or relaxed floating-point behavior requires an explicit numerical
  contract, isolated scope, and tests.
- Caching requires explicit ownership, invalidation, memory limits, thread
  safety, and eviction policy.

An optimized implementation is accepted only when the simpler version fails a
real requirement, the benefit is meaningful, correctness is testable, and the
complexity remains local.

## 17. Modules, Dependencies, and Boundaries

### Required

- Organize code into coherent modules with explicit, mostly acyclic dependency
  directions.
- Lower-level code must not depend on higher-level application policy.
- A module should expose a smaller interface than its implementation.
- Dependencies must be explicit and as narrow as practical.
- External data must be validated and converted into internal validated domain
  types at boundaries.
- Serialization, network protocols, public APIs, and plugins are contracts and
  require explicit versioning, ownership, error, layout, and compatibility
  rules.
- C++ ABI is not assumed stable across arbitrary compilers, standard libraries,
  or build settings.
- Exceptions must be caught before C or incompatible binary boundaries.
- Platform and third-party types should not spread through unrelated domain
  interfaces.

### Preferred

- Pass required long-lived dependencies through constructors and one-operation
  dependencies through function parameters.
- Prefer manual dependency injection over frameworks.
- Use concrete dependencies when one implementation exists and abstraction adds
  no real isolation.
- Use stable domain values, IDs, spans, options, and result types at major
  boundaries.
- Isolate platform, graphics, hardware, dynamic-loading, and third-party code in
  dedicated modules.
- Organize files by domain or component, not by generic technical category.
- Use namespaces for stable project and domain groupings without excessive
  depth.

### Restricted

- Avoid broad application contexts, service locators, singletons, mutable global
  state, and hidden static registration.
- Abstract interfaces require real runtime substitution, platform variation,
  external-service isolation, or plugin needs.
- Registries require dynamic discovery or extensibility and must define
  ownership, duplicate behavior, thread safety, and registration lifetime.
- C++20 language modules are not part of the default project structure.
- Pimpl does not create cross-toolchain ABI stability; use a stable C ABI or an
  explicit protocol when that is required.

## 18. Naming, Formatting, Comments, and Documentation

### Required

Use English for identifiers, source comments, API documentation, and developer
messages.

Default naming:

- `PascalCase` for classes, structs, enums, concepts, and aliases;
- `snake_case` for functions, variables, parameters, namespaces, and constants;
- trailing underscore for private non-static data members;
- `UPPER_SNAKE_CASE` only for macros;
- lowercase `snake_case` for filenames.

Further rules:

- Names must communicate purpose, not only type or implementation history.
- Boolean names should express a positive true condition.
- Include units or representation in names when the type does not express them.
- Collections should normally use plural names and elements singular names.
- Use an automated checked-in formatter configuration as the mechanical
  authority.
- Use braces for control-flow bodies.
- Do not reformat unrelated code during a functional change.

### Preferred

- Use verbs for actions and predicate-style names such as `is_`, `has_`, `can_`,
  or `should_` for boolean queries.
- Prefer noun accessors without a mechanical `get_` prefix.
- Use `find_` when absence is expected and `load_` or `read_` when I/O occurs.
- Keep lines near 100 characters as a practical target, not an absolute rule.
- Comments should explain reasons, invariants, external constraints, numerical
  assumptions, lifetime, threading, or non-obvious optimization.
- Document public contracts when ownership, failure, invalidation, units,
  performance, or thread behavior are not obvious.

### Restricted

- Avoid unusual abbreviations and private project jargon.
- Do not keep commented-out code.
- `TODO`, `FIXME`, and `HACK` comments must be concrete, actionable, and explain
  the removal condition when relevant.
- Do not use comments as a substitute for safe ownership, types, or interfaces.
- Do not generate ceremonial documentation for trivial private code.

## 19. Testing, Analysis, and Review

### Required

- A change is not complete merely because it compiles.
- Tests must be deterministic, independent, and focused on observable contracts.
- Test expected failures, boundaries, resource cleanup, and invalid input where
  relevant.
- Persistent formats require round-trip, known-good sample, malformed-input,
  compatibility, and version tests as appropriate.
- Resource-owning types require cleanup and move tests.
- Concurrent code requires cancellation, shutdown, stress, and race-oriented
  validation.
- Performance-motivated complexity requires benchmarks.
- Project-owned code must compile without warnings under the reviewed warning
  configuration.
- Build important code with all supported compiler families.
- Review the complete final diff for accidental, unrelated, generated, or stale
  changes.
- Report exactly which builds, tests, analyzers, sanitizers, benchmarks, and
  manual checks were run and which were not.

### Preferred

- Use unit tests for isolated algorithms and value types.
- Use component tests for collaborating subsystem behavior.
- Use integration tests for real external boundaries.
- Use a small number of end-to-end tests for critical workflows.
- Prefer simple fakes over interaction-heavy mocks.
- Add a focused regression test for every reproducible corrected defect.
- Use static analysis, AddressSanitizer, UndefinedBehaviorSanitizer,
  ThreadSanitizer, fuzzing, and property-based tests where their risk coverage is
  valuable.
- Use coverage as a diagnostic, not a target by itself.

### Restricted

- Do not make tests depend unnecessarily on private layout, exact internal call
  sequences, container capacity, or incidental implementation details.
- Do not hide flaky tests through automatic retries.
- Do not enable every compiler or analyzer warning indiscriminately; maintain a
  reviewed useful set.
- Suppress warnings only locally and only when the code is known correct.
- AI-generated code and tests require the same engineering review as human work,
  with special attention to invented APIs, ownership, portability, and tests
  that merely mirror the implementation.

## 20. Policy Enforcement and Final Checklist

### Required

This document defines project defaults. It must reduce complexity, not create
ceremony or artificial workarounds.

When rules appear to conflict, use this priority order:

1. correctness and safety;
2. explicit ownership and lifetime;
3. external contracts and compatibility;
4. portability across the supported matrix;
5. local comprehensibility and maintainability;
6. required performance;
7. stylistic consistency.

Existing code may not fully comply. Do not perform unrelated modernization, but
do not treat legacy patterns as approved defaults for new code.

A policy exception is valid only when:

- the concrete requirement is identified;
- the simpler compliant alternative was considered;
- the benefit justifies the added complexity;
- the deviation is local;
- ownership, lifetime, failure, and portability remain clear;
- the behavior is appropriately tested;
- the reason is documented when it would not be obvious later.

External, generated, and third-party code may follow different internal styles,
but project wrappers and boundaries must follow this policy.

### Project vocabulary

Use one established project mechanism for recurring concerns such as:

- results and errors;
- assertions;
- logging;
- ownership wrappers;
- task execution and cancellation;
- event subscriptions;
- serialization;
- platform abstraction.

Do not introduce competing abstractions for the same concern without a clear
project-wide decision.

### Final implementation checklist

Before considering a change complete, verify:

#### Scope and simplicity

- The change solves the requested problem and contains no unrelated redesign.
- New abstractions, dependencies, and generic mechanisms are necessary.
- Main control flow is easy to follow.
- Advanced features reduce total complexity rather than only line count.

#### Types and interfaces

- Types express important domain meaning and invalid states.
- Parameters expose mutation, optionality, and ownership.
- Return values expose success, absence, failure, and ownership.
- Public interfaces do not leak unnecessary implementation details.

#### Ownership and object design

- Every resource has one understandable ownership model.
- Values are used where practical.
- Dynamic and shared ownership are genuinely necessary where present.
- Views and observers cannot outlive owners.
- Construction establishes valid state and operations preserve invariants.

#### Errors and boundaries

- Programming errors, absence, expected failures, and exceptions are separated.
- External input is validated.
- Exceptions cannot escape forbidden boundaries.
- Error context is useful without duplicate logging.
- ABI, serialization, and protocol assumptions are explicit.

#### Genericity and language complexity

- Genericity is required by real uses.
- Constraints and diagnostics are understandable.
- Macros, callbacks, inheritance, ranges, and compile-time machinery do not hide
  control flow or lifetime.
- A simpler concrete solution was considered.

#### Concurrency and performance

- Concurrency has an owner, cancellation, shutdown, and synchronization model.
- Optimization is based on a real requirement and representative measurement.
- Specialized code is isolated and portable behavior remains available.
- Numerical and deterministic behavior remain acceptable.

#### Validation

- Affected targets compile with relevant supported toolchains.
- Relevant tests and failure cases pass.
- Warnings, analyzers, and sanitizers were reviewed as appropriate.
- Benchmarks were run when performance motivated complexity.
- The final diff contains no accidental changes.
- Validation reporting is factual and complete.

## Final decision rule

When several implementations satisfy the requirements, choose the one that is
easiest to understand locally, safest to modify, most predictable across
supported toolchains, and least dependent on hidden behavior.

The best implementation is not the one that uses the most modern C++ features.
It is the one that uses modern C++ most effectively while remaining ordinary,
explicit, efficient, portable, and easy to reason about.
