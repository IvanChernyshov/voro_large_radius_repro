# Stage 9 — A11/A12 extreme exponents, domain remapping, signed radii and near ties

This stage probes the numerical range of the construction and owner-search candidates, separating the accepted nonnegative-radius cases from signed-radius, near-tie and overflowing inputs.

## A11 — Native `find_voronoi_cell()` at extreme radius scales

The two new C++11 files `repro/extreme2d.cpp` and `repro/extreme3d.cpp` exercise **the actual public native API**, with an independently enumerated exact-algebra reference from the original binary64 radius inputs. Their acceptance subset covers nonnegative radii; negative radii are tested separately and **do not count as passing** even when the CI job is green.

Inputs include:

- Radius pairs with equal positive radii `1`, `1e8`, `1e154`, `1e200` and `1e308`.
- Pairs with an adjacent binary64 successor of `1e8`, `1e150`, or `1e308`.
- High-contrast `(0,R)` and `(R,0)` cases, with `R` at `1e8` and `1e150`, plus small-radius controls.
- Four **negative-radius diagnostic pairs** including two equal negative radii, all-negative unequal radii, and mixed signs.
- In 2D, all four rectangular periodicity masks. In 3D, all eight rectangular masks and the orthogonal/skew dedicated periodic containers.
- Spatial block grids 1, 2 and 4 per axis, both site insertion orders and query points inside and *outside* the primary domain (e.g. `x=-2.1, 2.3`). Queries outside a nonperiodic axis must return false; for periodic axes, the **returned image position**, not only particle ID, must be correct.

The reference checks the 5- and 6-image enumeration ranges for agreement. For numerical stability across GCC/Clang's extended `long double` and MSVC's binary64 `long double`, it evaluates the radius-gap term with `R=max(|ri|)` and a special case when `|ri|=R`:

```cpp
radius_gap = (abs(r) == R) ? 0 : (R-abs(r))*(R+abs(r));
```

The special case avoids `0 * infinity` when `r=1e308`. For the bounded test geometries, the maximal-radius site remains the unique finite-score owner when other shifted gaps overflow. The test excludes requests within `1e-12` of a reference score tie from mandatory correctness and reports such queries separately.

### Local results (GCC `-O2` and Clang `-O3`, Linux x86-64)

| Test subset | 2D | 3D |
|---|---:|---:|
| Nonnegative-radius required owner comparisons under candidate-03 | **0 errors / 20,958** | **0 / 58,296** |
| Outside-fundamental-domain queries (all radii, including diagnostics) | 0 incorrect / 12,096 | 0 / 34,128 |
| Negative-radius diagnostic comparisons | **1,184 incorrect / 6,300** | **3,176 / 16,968** |
| Nonzero but exceptionally small score-margin probes (diagnostic) | 0 incorrect / 1,764 | 7 / 42 |

In the seven 3D near-tie discrepancies, the nominal reference margin is only about `5.57e-18` and the difference concerns a query in a skew periodic lattice. That margin is far below a robust binary64 comparison at the tested geometric scale. Exact ties are also counted but never asserted as unique-winner queries. The tiny score margins prevent a reliable claim of a wrong owner at a clearly separated point.

The principal negative-radius counterexample is `r0=r1=-1e8` with sites `x=0.25, 0.75` in a unit domain. The exact power diagram is identical to the equal-positive-radius diagram, but candidate-03 may choose the wrong site depending on insertion order and block geometry. This follows from the original `max_radius` tracking the largest *signed* radius (initially zero) while the proposed shifted-score proof assumes `0 <= ri <= R`. It is **not** fixed by the combined patch.

The supported input contract for negative radii is not established by the native API documentation.

## A12 — Power-cell construction at extreme finite radius exponents

Two separate native C++ files `repro/exponent2d.cpp` and `repro/exponent3d.cpp` create **two equal-radius sites** at `x=0.25,0.75` in a unit nonperiodic domain, for 1, 2 and 4 blocks along each axis. The correct cells have individual measure 0.5, x-extents `[0,0.5]` and `[0.5,1]`, and sum 1.

| Radius | Construction under candidate-02 and experimental combined patch |
|---:|---|
| `1e8` | Correct |
| `1e154` | Correct |
| `1e200` | Correct |
| `1e308` | **Fails**: 2D `NaN` areas; 3D reports both cells empty |

The `1e308` case is deliberately **informational**, and its failures remain visible in every log. Although the input radius is finite, its square is **not** representable in binary64. A factored radius difference can still be evaluated correctly for equal radii, but `ri+rj` itself overflows before multiplication, giving `0*Inf = NaN`. The result is therefore a *different* failure regime from the earlier moderate-weight cancellation at `R=1e8`.

For profiles that repair construction, the three smaller exponents are **required** to pass. The highest exponent is recorded, not suppressed or called fixed.

## Independent GitHub Actions confirmation

[Stage 9 workflow run 41322933837](https://github.com/IvanChernyshov/voro_large_radius_repro/actions/runs/41322933837) on `34b8d81abfc9dda0d998aa1871bca1fa2a0e9955`. The results below cover four completed platform profiles; the later combined-patch tests also cover macOS ARM64 ([Stage 10](test-stage10-combined.md)).

In every completed `candidate-03` job, A11 reports **zero mismatches in 20,958 accepted nonnegative-radius 2D comparisons and 58,296 accepted 3D comparisons**. A11 also reports **zero wrong outside-domain remappings**. Negative-radius diagnostic mismatch counts differ slightly by compiler (e.g. GCC: 1,188 / 6,300 in 2D and 3,192 / 16,968 in 3D; Windows MSVC and Linux Clang: 1,184 and 3,176), so the particular count is not an invariant.

All `candidate-02` jobs report **zero A12 required failures**, together with **three explicitly diagnostic `1e308` geometry failures in 2D and three in 3D**. Candidate-03, which changes only search arithmetic, intentionally preserves the original A12 construction defects.

Near-tie classification also depends on arithmetic precision: in MSVC `long double` is binary64, and some margins that are distinguishable with extended precision instead appear as exact ties. No near-tie decision with gap `<=1e-12` is a mandatory pass condition.

## Current acceptance semantics

Run the source profiles with `bash ./run.sh <profile>` or `./scripts/run-msvc.ps1 -Mode <profile>`. A11 nonnegative queries are mandatory for candidate-03 and the later combined profile. A12 requires the three smaller exponents for candidate-02 and the combined profile. Signed-radius, near-tie and `1e308` cases remain diagnostic.

The same independent per-cell and per-owner checks used by A1–A10 remain in place. None of this constitutes an all-finite-double guarantee.

## Follow-up

[Stage 10](test-stage10-combined.md) combines candidate-02 and candidate-03 against a fresh original-source copy and tests the full construction and owner-search matrix. The signed-radius and `1e308` observations remain useful bounds on the demonstrated numerical range.
