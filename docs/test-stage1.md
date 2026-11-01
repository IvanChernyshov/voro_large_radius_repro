# Stage 1 — arithmetic regression probes (A1–A3)

**Status:** Research tests, not a claim that any proposed repair is complete. These tests make existing discrepancies measurable in the original Voro++ `master` as well as in the experimental `candidate-01`.

## Cases and analytic reference

The C++ files `repro/arithmetic2d.cpp` and `repro/arithmetic3d.cpp` run identical mathematical probes against the independent 2D and 3D `radius_poly` implementations.

- **A1: large equal radii.** Invoke the internal `r_scale(0.25)` with current and competing radii both `1e8`. Correct result: `0.25`, because the weight difference is exactly zero. A1 is also represented by the existing two-site geometric baseline.
- **A2: adjacent representable large radii.** Let `ri = 1e8` and `rj = nextafter(ri, +∞)`. For squared site separation `36`, the correct adjusted displacement is `36 + (ri-rj)*(ri+rj) = 33.019767761230469` (to the digits shown). Independently, place the sites at `(2,5)` and `(8,5)` in a nonperiodic `10 × 10` square, or at `(2,5,5)` and `(8,5,5)` in a `10 × 10 × 10` cube. Their power bisector has x coordinate `5 + (ri-rj)*(ri+rj)/12 ≈ 4.751647313435872`. The native cell boundary, its counterpart in the other cell, and both measures are checked.
- **A3: direct `r_scale_check` decision.** Using the A2 radii, input `rs=36`, `mrs=31`, correct adjusted `rs≈33.01976776`. Therefore `rs² < 31*36` and the candidate plane must **not** be skipped. A standalone subclass exposes only the protected `radius_poly` methods for this test. This checks internal arithmetic; it is **not yet** a full-container reproducer of a skipped plane.

All cases use C++11 and the original Voro++ sources; no Python or secondary tessellation implementation is involved. The analytic comparison uses a factored difference of squared input radii. In this particular example, the subtraction of two close positive binary64 radii is exact (Sterbenz lemma). The reference is not a generic arbitrary-precision evaluator for all magnitudes or overflow cases.

## Run

```bash
bash ./run.sh baseline
bash ./run.sh candidate
```

The original six-configurations-per-dimension matrix remains in place. The runner additionally compiles and executes the focused probes, records `output/<mode>/2d_arithmetic.txt` and `3d_arithmetic.txt`, and prints their individual PASS/FAIL results. Expected numerical failures at this research stage are **informational** for both modes; unexpected program termination or internal-oracle failure stops the runner.

## Local verification (Linux x86-64)

Using the local Voro++ source copy, whose relevant arithmetic and compute source blobs match the pinned original `master`, this batch was compiled with GCC and Clang and exercised against the isolated `candidate-01` patch. Before adding any new repair:

| Case | Original 2D / 3D | `candidate-01` 2D / 3D |
|---|---|---|
| A1 `r_scale(0.25)` equal radii | FAIL (returns `0`) | PASS |
| A2 `r_scale(36)` adjacent radii | FAIL (returns `34`) | FAIL |
| A2 real two-site boundary | FAIL (returns `x≈4.8333333`) | FAIL |
| A3 scaled value and cutoff decision | FAIL (`34`, skips plane) | FAIL (`34`, skips plane) |

This is a reproducible failure record, **not** an acceptance check for a corrected implementation. The subsequent phase will add location-query regressions (A4–A5) without mixing them with plane-construction tests; wider container/periodicity coverage comes later.
