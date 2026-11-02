# Stage 11 — Scope-only audit of radial pruning arithmetic

This stage examines scalar rounding and overflow in the remaining radial pruning operations, following the combined-patch tests. Its numerical probes are kept separate from the cell and owner-search acceptance matrices.

## 1. Questions investigated

The pinned original Voro++ `master` has near-identical 2D/3D implementations in `rad_option.hh`:

```cpp
r_mul = (r - max_radius)*(r + max_radius);
r_ctest(crs,mrs):  cc = crs + r_mul;
                   return cc > 0 && cc*cc > mrs*crs;
r_prime(rv):       r_val = 1 + r_mul/rv;
r_cutoff(lrs):     return lrs*r_val;
```

For a nonnegative radius `r` and an accurately tracked maximum `R >= r`, the exact correction `delta = r² - R² <= 0`. The `r_ctest` formula is a conservative *geometric block-exclusion* predicate only when the arithmetic inequality is reliable; `r_prime/r_cutoff` feed corner/edge/face block-intersection tests in `v_compute`. These routes are distinct from the already tested plane-construction path (`r_scale`) and native owner search (`r_current_sub`).

The scalar inputs test the arithmetic predicates directly. Whether a particular input arises in a realizable container geometry is a separate question.

## 2. Reproducible native probe

`repro/radius_bounds.cpp` exposes protected `radius_poly` methods through a small subclass, using precisely the original 2D or 3D header. It contains seven `r_ctest` probes and eight `r_prime/r_cutoff` probes. The source compiles in both dimensions under standard C++11 and does **not** change Voro++.

Examples:

| Probe | Native result | Mathematical/reference interpretation |
|---|---|---|
| `normal_true` / `normal_false` | Expected decisions | Ordinary scalar controls |
| `weight_dominates` | Does not prune | Expected for `r=0, R=1e8, crs=16` |
| `boundary_rounding_unit` | **Prunes** (`true`) | Exact binary64-input algebra gives **false**, but the margin is only about `9.7e-17` relative to the comparison scale |
| `boundary_rounding_large` | **Prunes** (`true`) | Exact binary64-input algebra gives **false**, with relative margin about `1.6e-17` |
| `square_overflow_conservative` | Does **not** prune | Exact comparison would prune; both products overflow to infinity, so this example performs unnecessary work rather than missing a candidate |
| `equal_zero_denom` | `NaN` | `r_mul=0` and `r_prime(0)` evaluates `0/0` |
| `distinct_zero_denom` | `-Inf` | The correction is negative and `rv=0` |
| `distinct_zero_product` | `NaN` | `r_cutoff(0)` multiplies `0 × -Inf` |
| `huge_distinct` | `-Inf` | For `R=1e200, r=0`, the intermediate factored radius-square difference overflows |
| `huge_equal_radius` | `NaN` | At `R=r=1e308`, even the factor `r+r` overflows, giving `0 × Inf` |
| `signed_negative_scope_only` | Very large positive cutoff | For `r=-1e8, max_radius=0`, the assumption `delta<=0` does not apply |

**Near-boundary reference calculation.** The two `r_ctest` counterexamples use ordinary representable binary64 input values. Their mathematically exact signs were checked independently using rational arithmetic on those precise binary input values, not by assuming the decimal spellings are exact. Writing `q=crs`, `M=mrs`, `D=(r-R)(r+R)`, the exact comparison is `q+D>0 && (q+D)² > M*q`.

- Unit case: `R=1`, `r=double(0.8)`, `q=1`, `M=double(0.40960000000000013)`. The exact left-minus-right difference is approximately **−3.97016e−17**, but the computed predicate returns true.
- Large case: `R=1e8`, `r=0`, `q=1.1805916207174113e21`, `M=1.1805716208021146e21`. The exact difference is approximately **−2.26609e25** against terms of order `1e42`, yet the native predicate returns true.

The two comparisons demonstrate ULP-scale scalar sign errors. A missing Voronoi facet would require a corresponding end-to-end geometric example.

## 3. Local source and numerical checks

The local files used for the probe match the Git blobs of original Voro++ `master` commit `b0dac575a47af0f90b5b100e6dc199a493c7cb83` for `src/rad_option.hh`, `2d/src/rad_option.hh` and both amalgamated C++ translation units. The probe source was compiled and executed in **both 2D and 3D** with GCC 14.2 `-O2 -std=c++11` and Clang 17 `-O2 -std=c++11`. All four executions reported the cases above with the same decision/finite-value classification.

A small *separate, temporary* instrumented-container check ran 800 attempts to compute 2D cells and 864 attempts to compute 3D cells with varied block grids, periodic flags and radii, including sites on selected grid boundaries. **No actual call with `rv=0` to `r_prime` was observed in that sample.** The finite sample does not exclude such a call in other geometries. Instrumentation was confined to temporary source copies.

## 4. Reproducing the scalar probes

The normal Bash and Windows PowerShell runners now compile and invoke the diagnostic after the established A1–A12 suites:

```bash
bash ./run.sh baseline
bash ./run.sh candidate-02
bash ./run.sh candidate-03
bash ./run.sh combined-02-03
```

Each profile records:

- `output/<profile>/2d_pruning_scope.txt`
- `output/<profile>/3d_pruning_scope.txt`

MSVC executes the same source with the appropriate dimension macro. These scalar diagnostics record mismatches and nonfinite values without changing the geometric acceptance gates.

## Cross-platform CI confirmation

[GitHub Actions run 41328535563](https://github.com/IvanChernyshov/voro_large_radius_repro/actions/runs/41328535563) tested commit `82fdfd79c1fb3bf0d2412d480f0076a3603c01b7`. At the time of this record, The results below cover five source profiles on Linux GCC, Linux Clang, macOS Clang and Windows x64 MSVC, all completed successfully. The later full cross-platform CI matrix, including macOS ARM64, is linked in the [README](../README.md).

In all four completed toolchain configurations the added 2D and 3D scalar diagnostic **compiled and executed**, printing `SUMMARY radius_poly dim=2 category=scope-only no_acceptance_claim=1` and the corresponding 3D line. Detailed probe values are retained as `2d_pruning_scope.txt` and `3d_pruning_scope.txt` within each profile's CI output artifacts.

The probe records numerical classifications separately; all previously established A1–A12 acceptance checks remained unchanged.

## 5. Interpreting the results

The same interpretation applies to the **negative-radius tests, extremely large exponent tests and near-tie tests in Stages 9–10**:

- **Negative radius:** power geometry depends on squared radii, whereas the source tracks the maximum signed radius. The supported semantics of negative input radii remain unclear.
- **Equal radii `1e308`:** the input radii are finite, but intermediate binary64 squares and sums overflow. These failures occur outside the demonstrated `1e8` cancellation correction.
- **Near ties / boundary rounding:** the scalar decisions can change within roughly one ULP. A reliable geometric failure claim would require a corresponding container-level reproduction with an unambiguous result.
- **Moderate-radius errors (A1–A10):** these occur in actual 2D/3D containers and are checked by separate geometric acceptance tests.

The scalar probes delimit the tested numerical range of the two corrections. They do not motivate an additional arithmetic patch in this study.
