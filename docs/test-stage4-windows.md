# Stage 4 — Windows x64 / MSVC validation

This stage checks the existing A1–A7 reproductions on Windows x64 with MSVC, using the pinned Voro++ `master` snapshot.

## Toolchain and runner

- GitHub Actions: `windows-2022`, x64; Microsoft Visual Studio 2022 Enterprise, MSVC toolset `14.44.35207` (Developer Command Prompt `VSCMD_VER=17.14.43`).
- Original source: Voro++ `master` commit `b0dac575a47af0f90b5b100e6dc199a493c7cb83`, unchanged inside `vendor/voro/`.
- Compilation: `cl.exe` with `/O2 /EHsc /std:c++14`; MSVC accepts the project's C++11-level native drivers under this newer language mode.
- Entrypoint: `scripts/run-msvc.ps1` with `-Mode baseline` or `-Mode candidate` in PowerShell (`pwsh`), keeping the MSVC `link.exe` ahead of the Git Bash utility of the same name.
- MSVC environment comes from `ilammy/msvc-dev-cmd@v1`; source files and the experimental patch are the same as for Linux/macOS. The candidate is applied by `git apply` to a disposable copy outside `vendor/voro/`.

## Verified Windows CI evidence

**Authoritative successful run:** [GitHub Actions run 41308568367](https://github.com/IvanChernyshov/voro_large_radius_repro/actions/runs/41308568367), source commit `8e6c5b36a42f2acbec07edb324cd2a79237579b5`. The **Windows x64/MSVC baseline and candidate jobs both completed successfully**; their output artifacts provide the per-test logs.

| Diagnostic | Original Voro++ | `candidate-01` |
|---|---:|---:|
| Original simple scale matrix (`repro2d`/`repro3d`) | 4 high-radius failures (diagnostic) | 0 |
| A1 equal-radius `r_scale` | Fail, returns 0 instead of 0.25 (2D/3D) | Pass (2D/3D) |
| A2 adjacent-large-radius boundary | Fail, x≈4.8333333333 instead of 4.7516473134 | Same failure |
| A3 direct `r_scale_check` | Wrong value and wrong decision | Same failure |
| A4–A5 owner queries (each dimension: 162 queries) | 21 wrong; zero control failures | Same 21 wrong |
| A6–A7 2D construction (216 cases) | 66 incorrect; zero control failures | 0 incorrect |
| A6–A7 3D construction (540 cases) | 174 incorrect; zero control failures | 0 incorrect |

These values match the high-radius failure modes observed with Linux GCC/Clang and macOS Clang, although previous Linux/macOS location runs sometimes reported 20 rather than 21 wrong owners; the exact failure count is compiler/platform dependent.

The baseline job passes when its controls succeed and the expected numerical discrepancies are recorded. The A1–A5 executables return code 1 for these known defects. The Windows wrapper treats this as diagnostic, but treats invalid oracles, control regressions, crashes, bad compilation and failed candidate A6–A7 as fatal. Explicit script exit code 0 after successful validation prevents PowerShell from incorrectly propagating the last *expected* native exit 1.

## Local Windows execution

In a Visual Studio Developer PowerShell with `cl.exe` and `git.exe` available:

```powershell
pwsh -File ./scripts/run-msvc.ps1 -Mode baseline
pwsh -File ./scripts/run-msvc.ps1 -Mode candidate
```

The scripts write individual measurements under `output/<mode>/`, and the GitHub workflow uploads them as artifacts `measurements-windows-msvc-o2-baseline` and `measurements-windows-msvc-o2-candidate`.

## Bring-up incidents, distinct from numerical failures

The first Windows job used `/TP` indiscriminately, causing MSVC to interpret a precompiled `.obj` file as source text. The build invocation was corrected to `/Tp<source.cc>` for the original translation unit and no global `/TP` when linking the C++ driver with that object.

A subsequent baseline job compiled and executed all regressions, but GitHub marked the step failed because the PowerShell caller inherited the diagnostic exit code 1 from the last native program. Explicit successful exit status resolved this. Neither infrastructure correction changed any Voro++ source file, test geometry, oracle, or candidate patch.

## Scope of this Windows test

- This stage adds only **one Windows toolchain/architecture**, not Windows ARM64 or MinGW.
- A6–A7 tests use **equal radii** and do not certify the handling of unequal large weights.
- A4–A5 cover only nonperiodic location queries; periodic location/image identity remains a separate test-design task.
- This stage tests candidate-01; the factored construction and owner-search corrections were evaluated later.
- Cross-platform results for the combined correction are summarized in [Stage 10](test-stage10-combined.md).

The Windows runner has continued to exercise the full `master` regression matrix, including the combined patch; see the [current README](../README.md).
