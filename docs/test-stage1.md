# Stage 1 — arithmetic regression probes (A1–A3)

These first probes compare the pinned Voro++ `master` with candidate-01, the initial change to plane-displacement arithmetic.

## Cases and analytic reference

The C++ files `repro/arithmetic2d.cpp` and `repro/arithmetic3d.cpp` run identical mathematical probes against the independent 2D and 3D `radius_poly` implementations.

- **A1: large equal radii.** Invoke the internal `r_scale(0.25)` with current and competing radii both `1e8`. Correct result: `0.25`, because the weight difference is exactly zero. A1 is also represented by the existing two-site geometric baseline.
- **A2: adjacent representable large radii.** Let `ri = 1e8` and `rj = nextafter(ri, +∞)`. For squared site separation `36`, the correct adjusted displacement is `36 + (ri-rj)*(ri+rj) = 33.019767761230469` (to the digits shown). Independently, place the sites at `(2,5)` and `(8,5)` in a nonperiodic `10 × 10` square, or at `(2,5,5)` and `(8,5,5)` in a `10 × 10 × 10` cube. Their power bisector has x coordinate `5 + (ri-rj)*(ri+rj)/12 ≈ 4.751647313435872`. The native cell boundary, its counterpart in the other cell, and both measures are checked.
- **A3: direct `r_scale_check` decision.** Using the A2 radii, input `rs=36`, `mrs=31`, correct adjusted `rs≈33.01976776`. Therefore `rs² < 31*36` and the candidate plane must **not** be skipped. A small subclass exposes the protected `radius_poly` methods. A3 checks the numerical branch decision directly; it does not establish a missed face in a full container.

The C++11 probes compare native arithmetic with the factored difference of the binary64 input radii. For these nearby positive radii, the subtraction is exact by Sterbenz's lemma. The reference applies to these inputs and does not cover arbitrary overflow cases.

## Run

```bash
bash ./run.sh baseline
bash ./run.sh candidate
```

The original six-configurations-per-dimension matrix remains in place. The runner additionally compiles and executes the focused probes, records `output/<mode>/2d_arithmetic.txt` and `3d_arithmetic.txt`, and prints their individual PASS/FAIL results. The initial `baseline` and `candidate` profiles record numerical discrepancies. Unexpected termination or an invalid reference fails the runner.

## Local verification (Linux x86-64)

The locally compiled source files match the corresponding blobs of the pinned `master` snapshot. GCC and Clang gave the following results for the original code and candidate-01:

| Case | Original 2D / 3D | `candidate-01` 2D / 3D |
|---|---|---|
| A1 `r_scale(0.25)` equal radii | FAIL (returns `0`) | PASS |
| A2 `r_scale(36)` adjacent radii | FAIL (returns `34`) | FAIL |
| A2 real two-site boundary | FAIL (returns `x≈4.8333333`) | FAIL |
| A3 scaled value and cutoff decision | FAIL (`34`, skips plane) | FAIL (`34`, skips plane) |

The results show that reordering already-squared radii fixes equal-radius cancellation but leaves the adjacent-radius error. [Stage 5](test-stage5-candidate02.md) tests the factored replacement; [Stage 2](test-stage2.md) examines the separate owner-search path.
