# Contributing to Elf3D

Focused bug fixes, compatibility improvements, tests, and documentation updates
are welcome.

## Before Submitting a Change

1. Keep the change limited to one clear purpose.
2. Preserve the separation between the engine, Standard Application Framework,
   explicit host integration, and viewer application.
3. Add or update tests for behavior changes.
4. Update the relevant public documentation.
5. Preserve all third-party copyright and license notices.

Check project-owned C++ with the CI-pinned `clang-format` 23.1.0 before
submitting:

```powershell
.\tools\check-format.ps1
```

## Build and Test

```powershell
.\tools\test.ps1 -Profile Full -Configuration Debug -Build -Headers
```

Run Release as well when changing compiler configuration,
optimization-sensitive code, packaging, or runtime deployment:

```powershell
.\tools\test.ps1 -Profile Full -Configuration Release -Build -Headers
```

For viewer or rendering changes, launch the viewer and describe the manual
checks performed.

## Pull Requests

Describe:

- the user-visible problem and result;
- public API impact;
- tests and manual checks;
- documentation changes;
- dependency or license impact.

Do not include confidential models, customer data, credentials, private file
paths, generated build output, or assets without clear redistribution rights.

Contributions to Elf3D source are provided under the repository MIT License.
