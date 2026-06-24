# Elf3D 0.1.0 Remediation Plan

Date: 2026-06-23

Repository: `Z:/Elf3D`

Branch: `audit/0.1.0`

Audit: `docs/audits/ELF3D_0.1.0_AUDIT.md`

Validation: `docs/audits/ELF3D_0.1.0_VALIDATION_MATRIX.md`

## Purpose

This plan orders the 0.1.0 audit findings by release risk and records whether
each finding will be corrected in code, corrected in documentation, or deferred.

## Remediation Order

1. Correct ownership and lifetime risks.
2. Correct or document public API diagnostics gaps.
3. Correct release documentation gaps and project-state baseline.
4. Correct documentation mismatches around glTF alpha, ABI compatibility, and
   OpenGL shutdown requirements.
5. Rerun Debug and Release configure/build/test validation after code changes.
6. Perform manual viewer validation before release-tag readiness is claimed.

## Finding Decisions

| Audit ID | Severity | Decision | Rationale | Validation Requirement |
| --- | --- | --- | --- | --- |
| AUD-001 | Critical | Correct documentation | The baseline artifact is missing; create a truthful `PROJECT_STATE_EN.md` from audited facts. | Documentation review and link/path checks. |
| AUD-002 | Informational | Correct documentation later | CMake/CTest were found in Visual Studio but not on `PATH`; document this in testing docs if useful. | No code validation required. |
| AUD-003 | High | Correct implementation | Scene destruction should not retain a raw engine-implementation pointer across the DLL ownership boundary. | Rebuild Debug/Release and run CTest. Add a public lifetime regression smoke. |
| AUD-004 | Medium | Defer implementation, document limitation | Public import-warning reporting is useful but would change the public API. For 0.1.0, keep behavior and document warnings as `std::clog` diagnostics. | Documentation review. |
| AUD-005 | Medium | Correct documentation | Opaque-only rendering is intentional for 0.1.0; docs must state that glTF base-color alpha is ignored. | Documentation review. |
| AUD-006 | Medium | Correct documentation | The 0.1.0 DLL surface is a C++ API with STL types; docs must state compiler/CRT compatibility expectations. | Documentation review. |
| AUD-007 | Medium | Correct documentation | OpenGL resource lifetime already has code-side guards and public comments; release docs should make the shutdown order explicit. | Documentation review. |
| AUD-008 | Medium | Correct documentation | The release documentation set is part of Goals 5-7. | Documentation review. |

## Implementation Plan for AUD-003

Replace `Scene`'s raw callback context with a private release context:

- `Scene::ReleaseContext` stores a weak state token and a callback.
- `Scene::Impl` owns a `std::shared_ptr<ReleaseContext>`.
- On scene destruction, the release context locks its weak state token.
- If the engine state is still alive, the callback releases renderer and
  picking caches.
- If the engine has already been destroyed, the weak token is expired and
  scene destruction becomes a safe no-op.

The public ownership rule remains: hosts should destroy scenes before the
creating engine. The implementation no longer turns that violation into a raw
engine-pointer dereference.

## Deferred Items

- AUD-004 public warning reporting is deferred to a future API revision unless
  release review decides it is required for 0.1.0.
- Manual visual viewer validation is deferred until after remediation and final
  documentation are complete.

## Validation Commands After Code Changes

Use Visual Studio's bundled CMake/CTest executables if they are still absent
from `PATH`:

```powershell
cmake --fresh --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug --output-on-failure
cmake --fresh --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release --output-on-failure
```
