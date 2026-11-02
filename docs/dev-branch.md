# Voro++ `dev`: numerical correction and validation

The Voro++ `dev` branch reorganizes the 2D and 3D implementations, so the
corrections developed against `master` require a separate patch. The same
cell-construction and owner-search errors occur in the active `dev` code.

## Source revisions

| Branch | Snapshot | Commit | Git tree SHA |
|---|---|---|---|
| `master` | `vendor/voro/` | `b0dac575a47af0f90b5b100e6dc199a493c7cb83` | `963387efecc4611d2103370ca8a6b03217e3efab` |
| `dev` | `vendor/voro-dev/` | `61099f682c6672a275b6be7de5923f2a1a7bf05e` | `67d734f48decce33d4844e150c27f5e6932a5979` |

Both source trees match the corresponding upstream Git trees. The `dev`
snapshot contains 238 files; it is structurally different from `master`.

## Minimal 2D and 3D reproductions

With generators at `x=0.25` and `x=0.75` in a unit box and equal
radii `1e8`, the expected cell measures are `0.5` and `0.5`.
The correct owner at `x=0.74` is generator `1`.

| Native operation | Original `dev` | Combined patch |
|---|---|---|
| 2D `compute_cell` | Areas `0.25, 0.25` | `0.5, 0.5` |
| 3D `compute_cell` | Volumes `0.25, 0.25` | `0.5, 0.5` |
| 2D `find_voronoi_cell` | Returns ID `0` | Returns ID `1` |
| 3D `find_voronoi_cell` | Returns ID `0` | Returns ID `1` |

The standalone programs are `repro/dev/minimal2d.cpp` and
`minimal3d.cpp`. The first baseline/patch comparison was recorded in
[CI run 41383927856](https://github.com/IvanChernyshov/voro_large_radius_repro/actions/runs/41383927856).

## Patch for the reorganized source

The active `dev` code builds both dimensions from `src/voro++.cc`.
Its `src/rad_option.hh` contains `radius_poly_2d` and
`radius_poly_3d`, with per-call radius values passed by reference.

[`patches/dev/candidate-02-plus-03.patch`](../patches/dev/candidate-02-plus-03.patch)
changes three files:

```text
src/rad_option.hh
src/v_compute_2d.cc
src/v_compute_3d.cc
```

For construction, `r_init` now provides the unsquared current radius,
and `r_scale`/`r_scale_check` use
`rs + (r_current-rj)*(r_current+rj)`.
For owner search, `r_current_sub` computes the score shifted by the
maximum squared radius; `r_max_add` becomes an identity. The search
derivation assumes nonnegative radii.

The current radius remains a local variable in `v_compute_2d.cc` and
`v_compute_3d.cc`, allowing concurrent calculations without a new
mutable container field. The pruning methods `r_mul`, `r_ctest`,
`r_prime` and `r_cutoff` are unchanged. The older `dev/2d/src/`
directory is preserved but is outside this active `dev/src/` patch.

## Full-container tests

For A9, irregular nine-site cells are compared individually with
direct clipping, including empty cells, centroids, extents and total
area/volume. A10 checks site IDs and periodic-image positions against
direct enumeration of power scores.

| Suite | Unmodified `dev` | Combined patch |
|---|---|---|
| A1–A3: equal/adjacent radii, plane adjustment | Incorrect values in 2D/3D | All pass |
| A9: 2D rectangular cells | 232 incorrect configurations / 384 | 384/384 |
| A9: 3D rectangular and dedicated periodic cells | 644–646 incorrect configurations / 960 | 960/960 |
| A10: 2D owner queries | Incorrect results | 36,288/36,288 |
| A10: 3D owner queries, including dedicated triclinic | Incorrect results | 38,880/38,880 |

Baseline failure counts depend slightly on compiler. The corrected
matrices pass on Linux GCC `-O2`, Linux Clang `-O3`, macOS ARM64
Apple Clang `-O2` and macOS Intel Apple Clang `-O3`.
[CI run 41401868846](https://github.com/IvanChernyshov/voro_large_radius_repro/actions/runs/41401868846)
covers the full A9 matrix, including 192 dedicated periodic 3D
configurations, on all four environments.

The 2D A9 oracle implements its own polygon clipping in `long double`.
The 3D reference enumerates cutting planes independently of block
search, but shares the low-level Voro++ polyhedron clipper. A10 enumerates
power distances and periodic images independently of accelerated
owner search. All are finite deterministic matrices.

### Dedicated triclinic cell construction: test harness correction

An early A9 extension and the isolated 40-case diagnostic initially
failed with `Constructing periodic image for nonexistent point`, even
for one generator of ordinary radius. The cause was the test loop:

```cpp
for (auto it=con.begin(); it!=con.end(); ++it)
    con.compute_cell(cell,it);
```

In `dev`, cell computation dynamically creates periodic image
particles. The `!= end()` loop can advance into those new image blocks
and attempt to compute their cells as though they were primary sites.
The native `dev` implementation uses `it < end()` for its own loops.

The corrected tests record the primary generators' block/particle
indices *before* constructing any cells and then compute cells by
those saved indices. This resolves the internal error without changing
Voro++ source code.

The initial erroneous iteration is retained as an isolated diagnostic
(`VORO_DEV_TRICLINIC_LEGACY_ITERATOR=1`); an instrumented temporary
copy records the out-of-range image index in
[CI run 41400336310](https://github.com/IvanChernyshov/voro_large_radius_repro/actions/runs/41400336310).

With primary-site enumeration, the 40-case study includes single and
paired sites, ordinary/large/adjacent radii, nine ordinary-radius
sites, orthogonal and skew lattices, different grids and insertion
orders:

| 3D dedicated periodic test | Original `dev` | Combined patch |
|---|---:|---:|
| Ordinary radii | 24/24 | 24/24 |
| Equal or adjacent large radii | 0/16 | 16/16 |
| **Total** | **24/40** | **40/40** |

[CI run 41401009657](https://github.com/IvanChernyshov/voro_large_radius_repro/actions/runs/41401009657)
records these results. The larger A9 matrix also uses primary-site
enumeration and passes 960/960 3D configurations with the patch.
[CI run 41401452701](https://github.com/IvanChernyshov/voro_large_radius_repro/actions/runs/41401452701)
first confirmed the expanded A9 on Linux GCC; the later cross-platform
run is linked above.

### Concurrency

A four-thread GCC/OpenMP test compares parallel and sequential cell
construction and owner queries in the corrected `dev`:

| Operation | Comparisons | Differences |
|---|---:|---:|
| 2D cells | 324 | 0 |
| 3D cells | 324 | 0 |
| 2D owner queries | 800 | 0 |
| 3D owner queries | 1,000 | 0 |

The test uses concurrent calls on shared containers with separate
per-call computation state. It checks these executions for differences;
it is not an exhaustive race analysis.
[CI run 41385277905](https://github.com/IvanChernyshov/voro_large_radius_repro/actions/runs/41385277905).

## Running the `dev` checks

```bash
bash ./scripts/reproduce-dev.sh             # four minimal original/patch comparisons
bash ./scripts/reproduce-dev.sh extended    # A1–A3, A9, A10
bash ./scripts/run-dev-openmp.sh            # OpenMP comparison
bash ./scripts/probe-dev-triclinic.sh       # additional dedicated-cell diagnostics
bash ./scripts/probe-owner-shift.sh         # known global-score-shift regression
```

The runner writes baseline and patched measurements to
`output/dev-baseline/` and `output/dev-combined/`. For the baseline
it verifies the known minimal failures and treats additional large-radius
mismatches as diagnostic data; the combined patch must pass the A9/A10
matrices. `VORO_DEV_A9_RECT_ONLY=1` restricts A9 to rectangular
3D containers when needed.

### Windows/MSVC

The original `dev` snapshot fails to compile under Visual Studio
2022 (`C2990` in `iterator` declarations in `container_2d.hh`,
`container_3d.hh` and `container_tri.hh`). Its numerical patch
has consequently been tested on Linux and macOS. The `master`
snapshot is tested under MSVC. The original compilation diagnostic
is recorded in
[CI run 41385735812](https://github.com/IvanChernyshov/voro_large_radius_repro/actions/runs/41385735812).

## Range of the results

The corrected `dev` code has been checked for the input families
above. The separate [owner-shift diagnostic](test-stage10-combined.md#global-shift-owner-search-regression)
also demonstrates a regression for **nonnegative** radii: the combined
search patch can lose a local small-radius distinction when a remote
site sets a much larger global radius. Negative radii, intermediate
overflow (including some `1e308` inputs), near-tie scores, and
rounding near pruning boundaries remain outside the tested guarantees;
see the [`master` arithmetic investigations](test-stage9-extremes.md)
and [pruning probes](test-stage11-pruning-scope.md).
