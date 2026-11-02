# Stage 5 — candidate-02: factored radius-square difference

Candidate-02 replaces the plane-displacement arithmetic in `master` with a factored difference of radii. This stage tests the construction path while leaving owner search unchanged.

## Scope and implementation

The new self-contained patch is `patches/candidate-02.patch`. It is applied to a temporary **unmodified original** Voro++ `master` copy, not on top of candidate-01. Only the parallel `src/rad_option.hh` (3D) and `2d/src/rad_option.hh` (2D) implementations of `radius_poly` change.

The original computes the plane displacement using two separately rounded squared radii:

```cpp
return rs + r_rad - rj * rj;  // r_rad = ri * ri
```

Candidate-01 groups the already-rounded squares before adding `rs`. This fixes the equal-radius example, but the near-equal example still loses substantial precision.

Candidate-02 instead saves the current **unsquared** radius in `r_init` and uses the factored difference in **both** `r_scale` and `r_scale_check`:

```cpp
const double rj = ppr[ijk][4*q+3]; // 3D; 2D uses [3*q+2]
return rs + (r_current-rj) * (r_current+rj);

// r_scale_check:
const double trs = rs;
rs += (r_current-rj) * (r_current+rj);
return rs <= 0 || rs*rs < mrs*trs;
```

The old private `r_rad` member is replaced with `r_current`. The existing factored calculation of `r_mul` is **not modified**. Neither `r_ctest`, `r_prime`, `r_cutoff`, `r_current_sub`, nor `r_max_add` is changed.

The mathematical identity is `ri² - rj² = (ri-rj)(ri+rj)`. With nearby binary64 input radii, subtraction of the radii can be exact even when subtracting separately rounded squares loses the meaningful difference. The factorization improves the tested nearby-radius cases; intermediate overflow and degenerate geometry are separate numerical questions.

## How to run the three independent versions

Linux/macOS:

```bash
bash ./run.sh baseline      # unmodified source; known failures are diagnostic
bash ./run.sh candidate     # original candidate-01, retained unchanged
bash ./run.sh candidate-02  # new self-contained replacement patch
```

Windows x64 (MSVC Developer PowerShell):

```powershell
pwsh -File ./scripts/run-msvc.ps1 -Mode candidate-02
```

The new profile **requires** A1–A3 arithmetic tests and all A6–A7 equal-radius container checks to pass; failed A4–A5 native location queries are still recorded as *known diagnostic errors* because candidate-02 intentionally leaves that search algorithm unchanged. Nonzero control-oracle exits remain fatal. All raw output is kept under `output/<profile>/` and uploaded as GitHub Actions artifacts.

## Analytic and local confirmation

The patch text was applied with `patch -p1` to the original 2D and 3D sources (whose relevant Git blob IDs match the pinned original master). Dedicated native C++ A1–A3 probes and a real two-site power-cell computation were compiled independently with GCC `-O2 -std=c++11` and Clang `-O3 -std=c++11` on Linux x86-64.

| Check, repeated in both dimensions | Original | Candidate-02 |
|---|---:|---:|
| A1: `r_scale(0.25)`, both radii `1e8` | `0` (wrong) | `0.25` |
| A2: adjusted `rs` with adjacent binary64 radii | `34` (wrong) | `33.019767761230469` |
| A2: two-site separator, exact oracle `x≈4.7516473134358721` | `4.833333333333333` (wrong) | `4.751647313435873` |
| A3: `r_scale_check`, `rs=36, mrs=31` | `false` (wrong) | `true` |

For A2, `r0=1e8`, `r1=nextafter(r0,+∞)`, with sites at x=2 and x=8 in a domain of length 10. Exact-algebra reference calculations use `(r0-r1)*(r0+r1)` on the *original binary64 radius inputs*. This is independent of the native Voro++ geometry.

## GitHub Actions verification

[CI run 41310649233](https://github.com/IvanChernyshov/voro_large_radius_repro/actions/runs/41310649233), revision `48fc8bc313bdf7cb46a866865dff63ad6379cd4a`. **At the time of this record 12/15 jobs completed successfully**, including all three profiles on the following four toolchain configurations:

| Candidate-02 environment | A1–A3 in 2D / 3D | A6–A7 2D | A6–A7 3D | A4–A5 wrong owners per dimension |
|---|---|---:|---:|---:|
| Linux / GCC `-O2` | all pass | 0 / 216 failures | 0 / 540 failures | 20 / 162 |
| Linux / Clang `-O3` | all pass | 0 / 216 | 0 / 540 | 21 / 162 |
| macOS / Clang `-O2` | all pass | 0 / 216 | 0 / 540 | 20 / 162 |
| Windows x64 / MSVC `/O2` | all pass | 0 / 216 | 0 / 540 | 21 / 162 |

This initial CI table covers the four completed toolchains. The later combined-patch matrix also covers macOS ARM64; see [Stage 10](test-stage10-combined.md). The exact wrong-owner count varies across compilers.

Original Voro++ continues to fail **66 of the 216 2D** and **174 of the 540 3D** A6–A7 cases at radius `1e8`, while candidate-01 and candidate-02 both pass that *equal-radius* geometry matrix. Only candidate-02 also repairs the A2 and A3 arithmetic failures.

## Scope of candidate-02

1. **Owner search:** `r_current_sub` and `r_max_add` are unchanged; A4–A5 still fail. Candidate-03 was developed separately in [Stage 8](test-stage8-candidate03.md).
2. **A3 is a direct numerical decision test:** it shows the corrected branch result for a chosen case, but not yet an end-to-end missed-face example attributable exclusively to `r_scale_check`.
3. **A6–A7 cover equal radii:** they do not independently validate all near-equal and substantially unequal multi-site 2D/3D/periodic geometries.
4. **No overflow/underflow guarantees:** the factored product can overflow or underflow for other radii even when a mathematical separator is finite. Existing global cutoff/pruning tests are not certified by this stage.
5. **Source isolation:** the change is distributed as a patch against the pinned original source.

### Subsequent verification

[Stage 6](test-stage6-unequal.md) checks unequal-radius cells, [Stage 7](test-stage7-stress.md) adds irregular multi-site geometry, and [Stage 10](test-stage10-combined.md) tests candidate-02 together with the independent search correction.
