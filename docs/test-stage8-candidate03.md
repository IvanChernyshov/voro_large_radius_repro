# Stage 8 — candidate-03: shifted-power location search

Candidate-03 addresses the native `find_voronoi_cell()` path in 2D and 3D. It is a separate patch against the original `master` source and leaves cell construction unchanged.

## 1. Why the original search loses information

The search implementation in `2d/src/v_compute_2d.cc` and `src/v_compute.cc` keeps a running minimum `mrs` initialized to `large_number`. For each particle, `scan_all()` obtains a score from `radius_poly::r_current_sub`. The original methods are:

```cpp
r_current_sub(rs, ...): return rs - r*r;
r_max_add(mrs):        return mrs + max_radius*max_radius;
```

The first expression subtracts a large squared radius from a small geometric squared distance. For radii around `1e8`, binary64 rounding can erase the geometric contribution entirely. The second expression does not restore the lost information.

The block worklists use `r_max_add(mrs)` as the threshold for geometric squared-distance lower bounds, including early worklist termination and `compute_min_radius()`. **Changing the candidate score without also changing this threshold would invalidate the pruning invariant.**

## 2. Exact-arithmetic derivation

Let the input radii be **nonnegative and finite**, and define `R = max_i r_i`, as tracked by Voro++ in `max_radius`. At a test point `x`, ordinary power distances are

```text
d_i = ||x - p_i||^2 - r_i^2.
```

Add the **same constant `R^2`** to every score, but compute the shift without subtracting separately rounded squared radii:

```text
s_i = d_i + R^2
    = ||x - p_i||^2 + (R - r_i)*(R + r_i).
```

In exact arithmetic this preserves the ordering and every nearest-power-site decision. Since `0 <= r_i <= R`, the added term is nonnegative. For any unvisited spatial block `B`, let `b_B` be a lower bound on squared Euclidean distance from `x` to any particle in `B`. Every particle in the block then has `s_i >= b_B`. Therefore the existing geometric pruning condition `b_B > min_i(s_i)` remains conservative and valid.

Consequently, **the original `r_max_add(mrs)` must become an identity** when `mrs` stores `s_i`, including all its early-exit and block-skip call sites. No changes to `v_compute` are needed: its calls and comparisons retain their mathematical meaning.

The replacement in `radius_poly` is:

```cpp
inline double r_max_add(double rs) {return rs;}

inline double r_current_sub(double rs, int ijk, int q) {
    const double r = ppr[ijk][radius_index(q)];
    if (r == max_radius) return rs;
    return rs + (max_radius - r)*(max_radius + r);
}
```

Here `radius_index(q)` stands for the original 2D/3D storage offsets; the actual patch uses `3*q+2` and `4*q+3` respectively. The equal-radius branch also prevents evaluating `0 * infinity` if `max_radius + r` overflows for very large but equal finite radii.

The derivation assumes nonnegative radii. The finite tests below examine its floating-point behavior away from nearly tied scores.

## 3. Patch contents

`patches/candidate-03.patch` modifies only **`radius_poly::r_current_sub` and `r_max_add`**, plus their documentation, in the two original `rad_option.hh` files. The existing `r_scale`, `r_scale_check`, `r_mul`, `r_ctest`, `r_prime` and `r_cutoff` computations are untouched. The patch was independently applied with `patch -p1` to a clean source copy and compared byte-for-byte with the locally tested modified headers.

Candidate-03 applies directly to the pinned original source, independently of candidate-02. The source tree retained under `vendor/voro/` has the same Git tree hash as original Voro++ `master`: `963387efecc4611d2103370ca8a6b03217e3efab`.

## 4. Native tests

Existing **A4–A5** contain 162 analytic two-site queries *per dimension*, including insertion-order and block-grid variation. Candidate-03 now **requires all 324 to pass**.

New `repro/owner2d.cpp` and `repro/owner3d.cpp` add **A10**, an entirely native, deterministic owner-query matrix using the irregular nine-site configurations and radius patterns from Stage 7:

| Property | 2D | 3D |
|---|---|---|
| Deterministic seeds / radius patterns | 4 / 6 | 4 / 6 |
| Periodicity modes | All 4 rectangular masks | All 8 rectangular masks + orthogonal/skew dedicated periodic containers |
| Spatial grids / insertion orders | 1, 2, 4 / forward and reverse | 1, 2, 4 / forward and reverse |
| Queries per configuration | 63 | 27 |
| **Native owner queries** | **36,288** | **38,880** |

Each query is checked against a **direct enumeration of candidate particles and their periodic images**, using `long double` arithmetic where available and the factored binary64-input radius difference. No Voro++ clipping or accelerated nearest-site search is used to establish the expected owner. The oracle must select the same site and **the same periodic image coordinates** for image ranges `[-1,1]` and `[-2,2]`; otherwise it exits with an oracle error (code 2). Queries close to a true tie are excluded with an explicit `1e-7` score-margin rule; none of these 75,168 queries triggered that exclusion on the platforms tested.

Returned `found`, site ID and shifted image coordinates are verified. Small-radius control failures or an invalid oracle are fatal in all profiles. Known high-radius mismatches in baseline/candidate-01/candidate-02 are **diagnostic**, whereas any mismatch in candidate-03 fails its CI job.

## 5. Local and independent cross-platform results

The locally compiled source has matching original Git blob IDs for `rad_option.hh` (2D/3D), `voro++.cc`, `voro++_2d.cc`, and the native compute implementations. GCC `-O2 -std=c++11` and Clang `-O3 -std=c++11` reproduced the errors on baseline and passed the new search patch in both dimensions.

**Authoritative GitHub Actions evidence:** [workflow run 41319325489](https://github.com/IvanChernyshov/voro_large_radius_repro/actions/runs/41319325489), source commit `d7a9b7b1292e152dde727d092069d24789258dce`.

| CI toolchain | Baseline A10 wrong 2D / 3D | Candidate-03 A10 wrong 2D / 3D | Candidate-03 A4–A5 wrong 2D / 3D |
|---|---:|---:|---:|
| Linux GCC `-O2` | 985 / 871 | **0 / 0** | **0 / 0** |
| Linux Clang `-O3` | 1181 / 878 | **0 / 0** | **0 / 0** |
| macOS Clang `-O2` | 985 / 871 | **0 / 0** | **0 / 0** |
| Windows x64 MSVC `/O2` | 1181 / 878 | **0 / 0** | **0 / 0** |

The baseline A4–A5 checks separately reported 20 or 21 wrong owners **per dimension**, depending on compiler, versus zero under candidate-03. Candidate-01 and candidate-02 retain the same owner-query failures as baseline in their respective CI jobs.

This table records the four platform profiles completed in the initial run; macOS ARM64 coverage followed in the combined-patch CI ([Stage 10](test-stage10-combined.md)).

Baseline error counts depend on the compiler. Baseline and construction-only profiles retain these errors as diagnostic results.

## 6. Separation from power-cell construction

Because candidate-03 changes only the location-query arithmetic, its A6–A9 results intentionally remain like unmodified Voro++, including the **66 incorrect A6–A7 2D configurations and 174 incorrect 3D configurations** at large equal radii. The observed A8/A9 individual-cell failures also remain. This non-interference was independently verified by the full CI run and locally for A8–A9.

Candidate-02 separately repairs cell construction. Their combined behavior was subsequently tested in [Stage 10](test-stage10-combined.md).

## 7. Limits of candidate-03

- The mathematical pruning proof assumes nonnegative radii and finite, representable intermediate values. The current original Voro++ code tracks the maximum **signed radius**, not necessarily the maximum absolute radius; negative radius inputs are outside this candidate's established assumptions.
- The factored product can overflow or underflow for some finite input values. The equal-radius branch avoids one problematic `0*infinity` case, but the broader extreme-exponent regime is not validated.
- Exact boundary ties, near-ties with tiny score margins, empty containers, queries outside fundamental domains, and pathological anisotropic geometry require further focused testing.
- The A10 enumeration is independent of Voro++'s location algorithm but limited to the stated finite image range and query matrix.
- Very large exponents and negative radii were examined in [Stage 9](test-stage9-extremes.md), with separate accepted and diagnostic cases.
- The combined construction/search patch was verified against both families in [Stage 10](test-stage10-combined.md).
