# Elf3D 0.1.0 Remediation Log

Date: 2026-06-23

Repository: `Z:/Elf3D`

Branch: `audit/0.1.0`

Audit: `docs/audits/ELF3D_0.1.0_AUDIT.md`

Plan: `docs/audits/ELF3D_0.1.0_REMEDIATION_PLAN.md`

## Purpose

This log records audit findings resolved during Goal 4 and the validation run
after each correction group.

## Resolved Findings

### AUD-002: CMake/CTest Were Not on PATH During Initial Audit

- Resolution: Resolved by locating Visual Studio's bundled CMake/CTest
  executables and completing Debug and Release validation.
- Changed files:
  - `docs/audits/ELF3D_0.1.0_AUDIT.md`
  - `docs/audits/ELF3D_0.1.0_VALIDATION_MATRIX.md`
- Tests added: none
- Tests executed:
  - `ctest --preset windows-debug --output-on-failure`
  - `ctest --preset windows-release --output-on-failure`
- Commit identifier: `fcf301c`
- Remaining limitations: `cmake` and `ctest` are still not on this shell's
  `PATH`; documentation can note the bundled Visual Studio paths.

### AUD-003: Scene Cache-Release Callback Had a Dangling-Context Failure Mode

- Resolution: Corrected implementation.
- Changed files:
  - `include/elf3d/scene.h`
  - `modules/scene/src/scene.cpp`
  - `facade/elf3d/src/engine.cpp`
  - `tests/public_api_test.cpp`
- Commit identifier: `7957aee`
- Summary:
  - Replaced `Scene`'s raw release callback context with a private
    `Scene::ReleaseContext`.
  - The release context stores a weak state token and no raw `Engine::Impl`
    pointer.
  - When a scene is destroyed while the engine state is still alive, renderer
    and picking caches are released as before.
  - When a scene is destroyed after the engine state is gone, the weak token is
    expired and destruction becomes a safe no-op.
  - Added a public lifetime smoke to `elf3d_public_api_test` that destroys an
    engine before a scene created from it, then destroys the scene.
- Tests added:
  - Public API lifetime smoke for late scene destruction after engine teardown.
- Tests executed after the final code state:
  - `cmake --build --preset windows-debug`
  - `ctest --preset windows-debug --output-on-failure`
  - `cmake --build --preset windows-release`
  - `ctest --preset windows-release --output-on-failure`
- Test result:
  - Debug: 16 passed, 0 failed
  - Release: 16 passed, 0 failed
- Remaining limitations:
  - The documented ownership contract remains that hosts should destroy
    `Scene` and `Viewport` objects before the creating `Engine`.
  - The regression smoke verifies safe facade destruction in the available test
    path, but it is not a sanitizer-backed proof of every out-of-contract
    lifetime sequence.
  - Manual viewer validation has not yet been performed after remediation.

## Deferred Findings

- AUD-001: Missing project-state baseline. Corrected in Goal 5 by creating
  `PROJECT_STATE_EN.md`; Goal 7 created the immutable release snapshot under
  `docs/releases/0.1.0/`.
- AUD-004: Public import-warning reporting. Deferred as an API enhancement for
  a future revision unless release review requires it for 0.1.0; document
  current `std::clog` behavior for 0.1.0.
- AUD-005: glTF base-color alpha behavior. Correct in documentation.
- AUD-006: C++ DLL ABI compatibility wording. Correct in documentation.
- AUD-007: OpenGL shutdown requirements. Correct in documentation.
- AUD-008: Living documentation set corrected in Goal 5; Goal 7 created
  `CHANGELOG.md` and the immutable release snapshot.
