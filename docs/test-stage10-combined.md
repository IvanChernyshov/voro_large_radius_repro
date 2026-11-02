# Stage 10 — independently combined candidate-02 + candidate-03

The combined candidate applies the construction and owner-search corrections to the same pinned `master` snapshot. The tests below compare both native operations with independent references on the stated input families.

## Construction and provenance

The proposed combined patch is `patches/candidate-02-plus-03.patch`. Its complete Git blob SHA-1 is `5ef59e80e642b74a03cde0aa01819e97cded4e28`.

It was generated as a single patch directly against `master` commit
`b0dac575a47af0f90b5b100e6dc199a493c7cb83`. It combines
candidate-02's factored plane displacement in `r_scale` and
`r_scale_check` with candidate-03's shifted owner score and matching
`r_max_add` threshold.

The combined patch changes only `src/rad_option.hh` and `2d/src/rad_option.hh`. The original source subtree remains **exactly** Git tree SHA `963387efecc4611d2103370ca8a6b03217e3efab`.

The combined patch was applied to a fresh copy of the original, verified with `patch --dry-run -p1`, compiled with GCC `-O2 -std=c++11` and Clang `-O3 -std=c++11`, and tested against A8–A12 locally. The standalone candidate-01, candidate-02 and candidate-03 patches were not modified or replaced.

## Run modes

```bash
bash ./run.sh baseline
bash ./run.sh candidate        # candidate-01
bash ./run.sh candidate-02
bash ./run.sh candidate-03
bash ./run.sh combined-02-03
```

Windows/MSVC: `./scripts/run-msvc.ps1 -Mode combined-02-03` in a Developer PowerShell. The combined profile applies `patches/candidate-02-plus-03.patch` to a **temporary** source copy, compiles the native 2D/3D implementations and all standalone harnesses, and writes the measurements to `output/combined-02-03/`. No Python dependency.

**Acceptance gates:** the combined profile requires every established A1–A10 construction/search regression to pass, the *nonnegative-radius* subset of A11 to pass, and the construction checks at radii `1e8`, `1e154` and `1e200` in A12 to pass. It does **not** require exact-tie or negative-radius owner queries, nor equal-radius construction at `1e308`, to pass: those are reported as active unresolved limitations.

## Independent CI results

[GitHub Actions run 41323213256](https://github.com/IvanChernyshov/voro_large_radius_repro/actions/runs/41323213256) for commit `ddcafd53f52632ee9da586ccf2ac6456d47c6dfb`. The results below record the four completed platform profiles in that run. The later completed cross-platform matrix, including macOS ARM64, is linked from the [README](../README.md).

| Mandatory regression suite, combined profile | Result on all four verified toolchains |
|---|---|
| A1–A3: scalar plane arithmetic and analytic two-site separator, both dimensions | Zero failures |
| A4–A5: 162 location queries in 2D + 162 in 3D | 324/324 correct |
| A6–A7: rectangular, partially periodic and skew dedicated cells | 756/756 configurations correct |
| A8: unequal-radius multi-site and empty-cell geometry | 588/588 correct |
| A9: noncollinear/noncoplanar multi-site geometry | 1,344/1,344 correct |
| A10: 36,288 2D + 38,880 3D native owner and periodic-image queries | 75,168/75,168 correct |
| A11: nonnegative-radius extreme-scale and out-of-domain owner queries | 20,958/20,958 (2D), 58,296/58,296 (3D) correct |
| A12: 1e8, 1e154, 1e200 equal-radius cell construction | 9/9 each dimension correct |

Verified environments:
- Ubuntu, GCC `-O2 -std=c++11`;
- Ubuntu, Clang `-O3 -std=c++11`;
- macOS, Clang `-O2 -std=c++11`;
- Windows x64, MSVC `/O2 /std:c++14`.

The original Voro++ and the three individual candidates retain their distinct, expected diagnostic patterns; baseline success indicates that the test controls and failure recording behaved as specified. The standalone construction candidate-02 still fails A4–A5/A10, and standalone search candidate-03 still fails A6–A9. This noninterference is a useful cross-check of the changes' separation.

## Unresolved numerical and API-contract limitations

### Negative radius inputs

The Voro++ `radius_poly` currently tracks `max_radius` as the **maximum signed input radius**, initialized to zero. Candidate-03's shifted-score pruning justification assumes `0 <= r_i <= max_radius`. For negative radii this may be false, despite the mathematical power diagram depending on (r_i^2), not the sign of (r_i).

With equal radii `-1e8` and sites at x=0.25 and 0.75, candidate-03/the combined candidate can return the wrong owner, although exact power geometry coincides with the equal-positive-radius case. A11 deliberately tests negative-radius pairs and **records failures instead of silently counting them as a pass**. Counts vary slightly by compiler; the completed combined-profile CI logs report roughly 1,184–1,188 wrong answers out of 6,300 2D diagnostic comparisons, and 3,176–3,192 out of 16,968 3D comparisons.

The Voro++ contract for negative sphere radii has **not** been established as a supported input; that question should be settled before extending or rejecting the patch on this basis.

### Very large but finite input radii

A12 conclusively shows that at `r=1e308` and in a unit domain, **both cells should occupy half the domain, but they do not**: Voro++ 2D produces `NaN` areas and 3D reports both cells empty. The combined and construction-only candidate-02 both fail the three `1e308` A12 grids per dimension; all three smaller tested exponents pass.

Even the factored product `(r_i-r_j)*(r_i+r_j)` can evaluate `0 * Inf = NaN` if the sum overflows, as can the existing factored `r_mul` in `r_init`. Importantly, `1e308` is a *finite radius* whose **square is not representable in binary64**. This regime must be explicitly distinguished from the original moderate-weight catastrophic cancellation where the squared radii themselves are finite.

The six `1e308` failures remain explicitly recorded as diagnostic results.

### Near ties

A11 excludes winner checks with a reference score gap `<=1e-12`, but still logs them. On extended-precision `long double` toolchains the 3D skew example `query=(-0.1,1.19,-0.1)` produces seven disagreements in 42 nonzero-but-extremely-small-gap diagnostic comparisons, with a reference margin near `5.57e-18`. This is not sufficient evidence of an unambiguous owner-selection defect at binary64 precision. On MSVC, `long double` is binary64, and the corresponding distinctions may round to exact ties; the diagnostic count should not be compared directly between platforms.

### Other numerical boundaries

The finite test matrices do not guarantee correctness for arbitrary power-weight differences, all scalar overflow/underflow scenarios, anisotropic or extremely large domains, numerical degeneracies, or periodic face-neighbor/image-shift metadata. The 2D source uses the finite `large_number=1e30` initialization for its search, so truly huge *geometric* coordinates deserve a dedicated follow-up rather than being assumed safe from the unit-domain tests.

Separately, the construction-pruning operations `r_ctest`, `r_prime` and `r_cutoff` have **not** received a complete adversarial rounding/overflow audit. The A3 direct arithmetic test confirms one corrected cutoff decision; it is not yet a full-container witness of a skipped plane caused solely by `r_scale_check`.

## Subsequent investigation

[Stage 11](test-stage11-pruning-scope.md) examines the remaining radial
pruning arithmetic near rounding and overflow boundaries. The same
construction and owner-search corrections were later adapted to the
reorganized [`dev` branch](dev-branch.md), including dedicated
triclinic cells. The unresolved input-range questions above remain
separate from the demonstrated errors at radii near `1e8`.
