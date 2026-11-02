# Voro++ development branch — separate fixed-SHA numerical reproductions

This document records the development-branch comparison **without replacing or changing the already validated master snapshot or patches**. The main repository README is deliberately not rewritten in this research pass.

## 1. Provenance and scope

| Source | Upstream commit | Exact Git subtree |
|---|---|---|
| Stable `master` in `vendor/voro/` | `b0dac575a47af0f90b5b100e6dc199a493c7cb83` | `963387efecc4611d2103370ca8a6b03217e3efab` |
| Development `dev` in `vendor/voro-dev/` | `61099f682c6672a275b6be7de5923f2a1a7bf05e` (17 October 2026) | `67d734f48decce33d4844e150c27f5e6932a5979` |

The `dev` snapshot includes 238 upstream files and exactly matches upstream file modes and Git blob/tree identities. All original source files and their original Voro++ license are retained. The `dev` branch is **not** a simple fast-forward relative to `master`, and the two branches should not share patch files blindly.

## 2. The same two native defects reproduce on dev

The minimal examples in `repro/dev/minimal2d.cpp` and `minimal3d.cpp` create two sites at `x=0.25` and `0.75` in a unit box with the same nonnegative radius `r=1e8`, then check both the cells and `find_voronoi_cell(x=0.74)`.

| API route and reference | Unmodified dev | Experimental combined dev patch |
|---|---|---|
| 2D `compute_cell`: total area 1, two individual areas 0.5 | Total area 0.5; two areas 0.25 | Pass |
| 3D `compute_cell`: total volume 1, two individual volumes 0.5 | Total volume 0.5; two volumes 0.25 | Pass |
| 2D `find_voronoi_cell`: query `x=0.74` should return ID 1 | Returns ID 0 | Pass |
| 3D `find_voronoi_cell`: same query | Returns ID 0 | Pass |

Both baseline reproduction and corrected behavior were independently established on Linux GCC, Linux Clang, macOS ARM64 Apple Clang and macOS Intel Apple Clang. **Expected wrong baseline outputs are tested explicitly**, rather than treating arbitrary native process failures as an acceptable reproduction.

See [CI run 41383927856](https://github.com/IvanChernyshov/voro_large_radius_repro/actions/runs/41383927856) for the initial independent minimal comparison.

## 3. Different code architecture, same numerical mechanism

The dev branch unifies most active 2D and 3D source under `vendor/voro-dev/src/`, including `src/voro++.cc`. Its `src/rad_option.hh` defines **two** classes, `radius_poly_2d` and `radius_poly_3d`, instead of storing a single `r_rad` member in the master version. Crucially, dev passes radius-related per-cell temporary variables **by reference** from `v_compute_2d.cc` and `v_compute_3d.cc` into the methods of `radius_poly`.

The original dev code still has two mathematically related cancellation routes:

- **Cell construction:** `r_scale` and `r_scale_check` compute `rs + r_i^2 - r_j^2`, with separately rounded squares and loss of geometric displacement. Their dev calls use a current-cell temporary.
- **Owner search:** `r_current_sub` compares `||x-p_i||^2-r_i^2`; `r_max_add` restores a huge maximum-radius square in block thresholds, too late to recover a lost small distance.

The standalone proposed dev patch, `patches/dev/candidate-02-plus-03.patch`, changes **only**:

```text
src/rad_option.hh
src/v_compute_2d.cc
src/v_compute_3d.cc
```

It saves the **unsquared current radius** in a *caller-local* `double r_current` variable and applies `(r_current-r_j)*(r_current+r_j)` in the two construction-plane methods. Separately, with nonnegative finite radii and `R=max(r_i)`, the location score becomes `||x-p_i||^2+(R-r_i)*(R+r_i)` and `r_max_add` becomes an identity. These are algebraically equivalent to the original power geometry for the specified input domain; the search shift preserves conservative geometric block pruning.

The caller's `r_rad` local variables are renamed `r_current` in both `v_compute_2d.cc` and `v_compute_3d.cc`. **No mutable shared current-radius field is introduced.** Existing `r_mul`, `r_ctest`, `r_prime` and `r_cutoff` computations remain unchanged.

The additional legacy `dev/2d/src/` directory is preserved byte-for-byte and is **not covered by this patch**. Validation targets the unified active `dev/src/` library build, as documented in upstream's development README and `src/voro++.cc`.

## 4. Separate full-container evidence

For the corrected dev source, the C++11 adapted drivers preserve the same selected oracles and inputs used in the master study.

| Suite | Dev baseline | Combined dev candidate |
|---|---|---|
| A1–A3, equal and adjacent binary64 radii, cut-plane geometry and scalar cutoff decision | Known failures in 2D and 3D | **All pass** |
| A10, irregular nine-site `find_voronoi_cell` with periodic-image coordinates | Wrong answers in both dimensions | **0 / 36,288 (2D); 0 / 38,880 (3D)** |
| A9, irregular nine-site `compute_cell`, all four 2D rectangular periodic masks | 232/384 failing configurations on GCC/Clang | **0 / 384** |
| A9, irregular nine-site `compute_cell`, all eight 3D rectangular periodic masks | 512/768 failing configurations on GCC/Clang | **0 / 768** |

The original A10 error counts vary with compiler (for example Linux GCC: 985 wrong in 2D and 871 in 3D; Linux Clang: 1181 and 878), as they do in the separately checked master version. Those counts are diagnostic, not expected universal constants. Ordinary-radius and oracle-validity controls passed.

The 2D A9 reference independently clips polygons in `long double`; the 3D A9 reference enumerates planes without search/block pruning but shares the primitive Voro++ low-level 3D convex clipping algorithm. A10 directly enumerates power distances and periodic images without Voro++'s owner-search implementation. These are **finite deterministic test matrices**, not proofs for all input geometries.

[CI run 41385014955](https://github.com/IvanChernyshov/voro_large_radius_repro/actions/runs/41385014955) confirms the selected A1–A3/A9/A10 results on Linux GCC and Clang and on both macOS architectures.

### Separate tri-periodic construction diagnostic

In **dev only**, the attempt to extend A9 to the dedicated `container_triclinic_poly::compute_cell()` route encountered a **native VOROPP_INTERNAL_ERROR (exit 3)** with the diagnostic:

```text
voro++: Constructing periodic image for nonexistent point
```

The very first tested case is *not* a huge-radius case: nine sites with ordinary nonuniform radii (A9 seed 0, pattern 0), the **orthogonal** dedicated periodic lattice (mask 8), one block per axis, original insertion order. The message occurs in **both** the original and patched dev code. It is not evidence of a regression caused by the proposed radius patch and is outside the present defect claim. The code path uses the dev-specific triclinic image-generation and cell-computation infrastructure.

As a result, A9 dev acceptance intentionally covers **2D 384 and 3D 768 rectangular configurations only**. The dedicated triclinic A9 scenario remains callable with `VORO_DEV_A9_TRICLINIC=1` for isolated diagnosis. The dev A10 **triclinic owner-query path remains in its full 38,880-query test**, and it passed for the corrected dev.

Do not report the dev patch as having passed a dedicated triclinic **cell-construction** test.

### Isolated dedicated-cell diagnostic (2 November 2026)

A further read-only comparison of the **original** and **combined-patched** dev sources is archived by
[CI run 41397139610](https://github.com/IvanChernyshov/voro_large_radius_repro/actions/runs/41397139610)
(commit `5bbe039`). The standalone `repro/dev/triclinic_probe.cpp` and
`scripts/probe-dev-triclinic.sh` execute each case in its own process, so a native
fatal error cannot hide later cases. Five scenarios (one ordinary-radius site;
two ordinary-radius sites; two equal large radii; two neighboring binary64 radii;
and the original nine-site ordinary-radius configuration) are crossed with
orthogonal/skew dedicated lattices, block grids `1`/`3` and insertion orders.

| Variant | Correct dedicated `compute_cell` cases | Same internal image-index error | Unexpected exits |
|---|---:|---:|---:|
| Unmodified dev | 0/40 | 40/40 | 0 |
| Combined-patched dev | 0/40 | 40/40 | 0 |

Every failing case reports `VOROPP_INTERNAL_ERROR` (native exit status 3):
`Constructing periodic image for nonexistent point`. Crucially, **even a
single site with a small radius fails** in this specific diagnostic setup.
These observations establish that the numerical patch is not needed to
trigger the native error. They do **not** establish the underlying cause,
the correctness of the test setup under every dev API convention, or that
all possible triclinic inputs fail. Zero geometry mismatches in this
diagnostic means **zero completed geometry comparisons**, not forty
successful comparisons.

This job is explicitly **diagnostic-only**, not part of the numerical
acceptance claim. The combined patch continues to pass the dedicated
triclinic **owner-query** cases within A10; it has **not** been validated
for dedicated triclinic **cell construction**. Further analysis of the
independent failure, if undertaken, must keep this distinction explicit.


## 5. Multithreading check

The dev combined patch retains per-call local radius state. An additional `-fopenmp` Ubuntu GCC test, `repro/dev/thread_safety.cpp`, exercised **four active OpenMP threads** and compared concurrent operations with sequential reference calculations on the same containers:

| Operation | Concurrent checks | Mismatches |
|---|---:|---:|
| 2D cells | 324 | 0 |
| 3D cells | 324 | 0 |
| 2D owner lookup | 800 | 0 |
| 3D owner lookup | 1,000 | 0 |

[CI run 41385277905](https://github.com/IvanChernyshov/voro_large_radius_repro/actions/runs/41385277905) contains the OpenMP result. This verifies the tested multithreaded execution, **not** the absence of races in all use cases.

## 6. Reproduction commands and interpretation

On Linux/macOS with a C++11 compiler and Bash:

```bash
# Strict minimal reference: 4 baseline native failures, 4 corrected passes.
bash scripts/reproduce-dev.sh

# Includes A1–A3, A9 rectangular construction and A10 owner-query tests.
bash scripts/reproduce-dev.sh extended

# Threaded exercise of a temporary patched dev source copy.
bash scripts/run-dev-openmp.sh
```

### MSVC is excluded from the dev validation matrix

An explicit native Windows x64 / Visual Studio 2022 test of the **unmodified dev** source `src/voro++.cc` was attempted with `/O2 /std:c++14`. Compilation fails **before any numerical tests are run**, with MSVC `C2990`: the nested `iterator` declarations conflict with `std::iterator` in `src/container_2d.hh` (line 204), `src/container_3d.hh` (line 211), and `src/container_tri.hh` (line 169). The headers contain the original upstream code.

This is an *independent dev compiler-compatibility issue*, not evidence of either a successful or unsuccessful radius fix. We have **not** introduced a source compatibility shim, patched upstream, or counted MSVC as a verified dev platform. Consequently, the main dev CI runs only GCC/Clang on Linux/macOS, plus the separate Linux GCC OpenMP test. The **pinned master** CI retains its existing successful Windows/MSVC coverage.

The observed failure is preserved in [the dev MSVC diagnostic CI run](https://github.com/IvanChernyshov/voro_large_radius_repro/actions/runs/41385735812). Support for dev on MSVC is outside the scope of the present numerical investigation.

For both source variants, the dev runner writes artifacts to `output/dev-baseline/` and `output/dev-combined/` (Windows has the `-msvc` suffix). A green original-baseline job means **known numerical errors were reproduced and controls passed**; it does **not** mean the original source produced correct power geometry. The corrected dev profile requires zero accepted failures.

## 7. Remaining limits and what not to ask of upstream

This dev adaptation targets precisely the moderate-regime cancellation demonstrated by both native APIs. As in the master investigation, **negative radii**, **input radius 1e308 and intermediate overflows**, **near ties**, and scalar ULP-boundary pruning effects describe limits of the tested arithmetic; they are **scope-only observations**, not a new requirement that Chris add range checks, accept negative radii, or change the library's numerical contract.

No upstream Voro++ source, issue or pull request was changed. The large root README should be rewritten only after master and dev provenance, test scope and branch-specific patch architecture have been reconciled into a simple technical narrative.
