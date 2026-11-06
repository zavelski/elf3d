# Testing Elf3D

Tests are separate from production solutions. Sources/fixtures live in `tests`;
test binaries and generated data stay under `out`.

```powershell
.\tools\test.ps1 -Profile Model -Configuration Debug -Build -Headers
.\tools\test.ps1 -Profile Full -Configuration Both -Build -Headers
.\tools\test.ps1 -Configuration Debug -Filter 'elf3d.model_' -List
```

`-Build` builds prerequisites and tests; otherwise existing outputs are used.
`-Filter` selects names by regular expression. Model covers the CPU document,
import/export and CLI closure. Full adds scenes, runtime, App, Viewer and graphics.
The single catalogue `tests/tests.json` retains arguments, timeouts, configuration
selection and skip codes. Fatal scenarios require an isolated failing child
process and the controlled diagnostic. A timeout is a failure.

`-Headers` compiles public/component contracts as first and repeated includes
without PCH, then links two combined units to check ODR. `-RequireGraphics`
rejects unavailable graphics contexts; otherwise such tests report a skip.
A skipped graphics check does not validate rendering. Real graphics need OpenGL 4.1.

Reports keep the latest success and failure per profile/configuration under
`out/reports/tests`. Product DLLs are loaded from x64; test-only providers stay
with tests. VS test projects are in `win/Elf3D-Tests.slnx`; developer tools,
captures, benchmarks and the environment baker are in `win/Elf3D-Tools.slnx`.

`tools/check-format.ps1` requires clang-format 23.1.0. `tools/check-build.ps1`
checks native build contracts, including duplicate literal file `Include` items
in every `.vcxproj`. Paths are compared case-insensitively after resolving them
relative to the project, including slash and `.`/`..` aliases. A file declared
under different item types also fails. MSBuild property/glob expansion and
`Update`/`Remove` entries are outside this static check. `-CheckFixtures` covers
valid metadata/dependency items and duplicate-file rejection.
Interactive visual checks remain separate;
compilation and automated tests do not claim manual GUI validation.

Maintainers run `tools/validate.ps1 -Configuration Both` for an immutable local
Git-tree snapshot, with CMake/CTest/CPack removed from PATH. The gate builds CPU
then Full in native and audited public source trees, verifies CPU cache reuse,
incremental builds, independent Viewer Clean/Rebuild, version resources,
external consumers, privacy rejection cases, and the extracted Release ZIP.
The successful gate removes its heavy scratch trees; failed trees retain
diagnostics. `-KeepArtifacts` retains successful scratch trees for investigation.
`tools/build.ps1 -NoPch` disables private production PCH for a focused build.
