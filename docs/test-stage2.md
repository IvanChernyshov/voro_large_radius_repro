# Stage 2 — independent native location-query checks (A4–A5)

This stage tests the native `find_voronoi_cell()` API in Voro++ `master`, independently of the cell-construction errors found in [Stage 1](test-stage1.md). The initial candidate-01 is retained for comparison.

## Scope

The standalone `repro/locate2d.cpp` and `repro/locate3d.cpp` use the unchanged, pinned original Voro++ `master` implementation and the same experimental `candidate-01` patch already in the project.

For **each dimension**, test four scenarios:

| ID | Generator positions along x | Radii | Reference |
|---|---|---|---|
| A4 / ordinary control | x=0.25, 0.75 in a unit box | Ordinary Voronoi (no radii) | boundary x=0.5 |
| A4 / small control | x=0.25, 0.75 in a unit box | both 1 | boundary x=0.5 |
| A4 / large equal | x=0.25, 0.75 in a unit box | both 10⁸ | boundary x=0.5 |
| A5 / adjacent large | x=2, 8 in a 10-unit box | 10⁸ and its next binary64 successor | boundary x≈4.751647313435872 |

All remaining coordinate components are set to the midpoint of the corresponding domain.

The analytic power bisector for two sites on the x axis with positions `a<b` is

`x_boundary = (a+b)/2 + (r0-r1)*(r0+r1)/(2*(b-a))`.

Computing the radius difference *before* squaring is essential for a reliable reference with adjacent large input radii. Here `r1=std::nextafter(1e8,+∞)`, and the code verifies the reference boundary independently of native Voro++ results. Queries are strictly inside the domain and stay at least `1e-4` away from the reference boundary. No tie-breaking behavior is tested.

The actual checks validate the `found` flag, the returned particle ID and the returned site coordinates. The reference result is obtained from the two-site bisector formula, without `compute_cell()`.

Each scenario is tested using:

- 1, 2 and 4 blocks per axis;
- both particle insertion orders, retaining stable explicit particle IDs;
- 6 query coordinates for each A4 scenario, 9 for A5.

That is **162 nonperiodic native location queries per dimension** (324 across 2D and 3D) per source variant. These first owner tests use nonperiodic rectangular domains. The periodic-image cases are covered in [Stage 8](test-stage8-candidate03.md).

## Verified local observations

With the pinned original arithmetic and the existing `candidate-01` patch, local Linux x86-64 runs gave identical counts under GCC `-O2` and Clang `-O3`:

| Case | Original 2D | Original 3D | Candidate-01 2D | Candidate-01 3D |
|---|---:|---:|---:|---:|
| Ordinary control, 36 queries | 0 wrong | 0 wrong | 0 wrong | 0 wrong |
| Small-radius control, 36 queries | 0 | 0 | 0 | 0 |
| A4 large equal radii, 36 queries | 8 | 8 | 8 | 8 |
| A5 adjacent large radii, 54 queries | 13 | 13 | 13 | 13 |
| **Total (162)** | **21** | **21** | **21** | **21** |

For example, with one block and forward insertion order, the original equal-large-radius query `x=0.74` returns site 0 (at `0.25`) rather than site 1 (at `0.75`). Reversing insertion order makes the first candidate effectively win for other queries. Block count also changes the erroneous results. The near-equal-large case gets site 0 at `x=4.8`, even though site 1 is the unique correct power owner because the exact boundary is at `4.751647313435872`.

### Independently observed GitHub Actions results

The first [Stage 2 workflow run](https://github.com/IvanChernyshov/voro_large_radius_repro/actions/runs/41299881690) reports a small but genuine **compiler/platform dependence** in the failure count. For both 2D and 3D, in both the original and candidate-01 profiles:

| CI configuration | A4 large-equal incorrect / 36 | A5 adjacent incorrect / 54 | Total incorrect / 162 |
|---|---:|---:|---:|
| Ubuntu, GCC `-O2` | 8 | 12 | 20 |
| Ubuntu, Clang `-O3` | 8 | 13 | 21 |
| macOS, Clang `-O2` | 8 | 12 | 20 |

All ordinary and small-radius controls passed on the three toolchains shown. The wrong-owner count varies slightly with compilation settings; the failure itself reproduces on each. The later cross-platform results appear in the [combined validation](test-stage10-combined.md).

The unchanged results with `candidate-01` are expected because that patch affects `r_scale()` for plane construction, not the `r_current_sub()` / `r_max_add()` arithmetic used by the location search. The candidate-01 comparison isolates the search fault because candidate-01 changes only cell construction. Candidate-03 subsequently changes the owner-search arithmetic; see [Stage 8](test-stage8-candidate03.md).

## Commands and failure semantics

```bash
bash ./run.sh baseline
bash ./run.sh candidate
```

Results: `output/<mode>/2d_location.txt` and `output/<mode>/3d_location.txt`. Each individual mismatch prints the scenario, block count, insertion order, query, expected and observed IDs, and returned coordinates. Each scenario prints a summary.

Exit code 0 means all queries matched. Exit 1 means only the known large-radius cases differed and is recorded as **diagnostic evidence**, not as an accepted result. Exit 2 means a control failed or the analytic oracle/test setup was invalid, and **does fail the runner**. The CI baseline/candidate jobs may be green while known high-radius tests contain explicit FAIL records; the machine-readable summaries and uploaded raw logs remain the source of truth.

## Important limitations

These tests establish incorrect native owner selection on an analytical two-site geometry. The candidate-03 derivation and periodic-image checks in [Stage 8](test-stage8-candidate03.md) identify the affected `r_current_sub`/`r_max_add` convention and verify the corresponding search correction.
