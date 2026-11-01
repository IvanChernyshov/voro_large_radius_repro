# Stage 2 — independent native location-query checks (A4–A5)

**Status:** Read-only Voro++ source audit plus new C++ regression tests. No fix implemented in this stage. These tests exercise the actual `find_voronoi_cell()` API, independently of the cell-construction path tested in Stage 1.

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

The actual checks validate the `found` flag, the returned particle ID and the returned site coordinates. There is **no call to Voro++'s `compute_cell()` for the reference result**.

Each scenario is tested using:

- 1, 2 and 4 blocks per axis;
- both particle insertion orders, retaining stable explicit particle IDs;
- 6 query coordinates for each A4 scenario, 9 for A5.

That is **162 nonperiodic native location queries per dimension** (324 across 2D and 3D) per source variant. This batch tests only rectangular, nonperiodic location queries; periodic-image owner matching and other container modes are deliberately reserved for later stages.

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

All the standard and small-radius controls passed. The macOS arm64 jobs were still queued at the time this record was written, so **no result for that configuration is asserted here**. The exact number of wrong owners is not an invariant: the supported conclusion is that the fault reproduces across all completed configurations, while the precise failure pattern varies with compilation.

The unchanged results with `candidate-01` are expected because that patch affects `r_scale()` for plane construction, not the `r_current_sub()` / `r_max_add()` arithmetic used by the location search. These tests do **not** evaluate or approve the proposed search arithmetic fix.

## Commands and failure semantics

```bash
bash ./run.sh baseline
bash ./run.sh candidate
```

Results: `output/<mode>/2d_location.txt` and `output/<mode>/3d_location.txt`. Each individual mismatch prints the scenario, block count, insertion order, query, expected and observed IDs, and returned coordinates. Each scenario prints a summary.

Exit code 0 means all queries matched. Exit 1 means only the known large-radius cases differed and is recorded as **diagnostic evidence**, not as an accepted result. Exit 2 means a control failed or the analytic oracle/test setup was invalid, and **does fail the runner**. The CI baseline/candidate jobs may be green while known high-radius tests contain explicit FAIL records; the machine-readable summaries and uploaded raw logs remain the source of truth.

## Important limitations

This phase establishes **wrong native owner queries**, not the precise internal operation responsible for every observed failure or the safety of any new fix. The source-level interpretation is that large absolute squared radii lead to cancellation in `r_current_sub` and `r_max_add`; causal intervention and the analysis of search-pruning invariants belong to a later candidate-fix stage.

The next project stages are the remaining container/periodicity coverage, a Windows MSVC build, and then independent tests of proposed arithmetic repairs. The root README is not yet promoted to a definitive explanation.
