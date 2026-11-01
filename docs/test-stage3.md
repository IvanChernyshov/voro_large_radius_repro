# Stage 3 — A6–A7 container and periodicity coverage

**Status:** Test coverage and characterization only. No new numerical fix is implemented or proposed as accepted here. The original pinned Voro++ `master` tree and `candidate-01.patch` remain unchanged.

## Scope and mathematical expectations

`repro/coverage2d.cpp` and `repro/coverage3d.cpp` test native `compute_cell()` through the principal regular and radical/power containers. Every test is executed using both insertion orders, 1/2/4 spatial blocks along each axis, and three modes: ordinary Voronoi, power with common radius 1, and power with common radius 10⁸.

In unit rectangular domains, the site coordinates along x are:

| Site count | x coordinates | Expected individual areas or volumes |
|---|---|---|
| 1 | `0.75` | `1` |
| 2 | `0.25, 0.75` | `0.5, 0.5` |
| 3 | `0.2, 0.5, 0.8` | `0.35, 0.30, 0.35` |

The other site coordinates are `0.5`. These fractions yield the same exact cell measures for **all** rectangular periodic-axis masks: 2D has four masks (`00, 10, 01, 11`) and 3D has eight. The tests verify the full-cell measure, total domain measure, cell IDs and x-extents. **A periodic cell is not required to lie inside the fundamental box:** the single site at x=0.75 with periodic x produces a valid cell spanning x=0.25 to x=1.25, which is what the analytic check expects.

In 3D we also test the dedicated `container_periodic` and `container_periodic_poly` for two lattices:

- orthogonal: `(bx,bxy,by,bxz,byz,bz) = (1,0,1,0,0,1)`;
- genuinely skew: `(1,0.23,1,-0.17,0.31,1)`.

Both have covolume 1. With one site, the cell must have volume 1. With two sites that differ by half the first lattice vector, translation by that half-vector exchanges the sites, so each must have volume 0.5. With three sites, the sum must equal 1 and inversion symmetry requires the outer two sites to have equal volumes. For this more involved skew geometry, the individual three-site power volumes are additionally compared against **the independently executed ordinary Voronoi container with the same sites**. This is a metamorphic comparison rather than a fully independent exact-arithmetic geometry oracle.

Thus Stage 3 checks both invariant geometry and whether errors persist across container implementations, not whether a computed periodic face has the correct image/shift ID.

## Matrix size

| Scope | Configuration count per source variant |
|---|---:|
| 2D rectangular (`container_2d`, `container_poly_2d`) | `4 periodic masks × 3 block sizes × 3 site counts × 2 orders × 3 modes = 216` |
| 3D rectangular (`container`, `container_poly`) | `8 × 3 × 3 × 2 × 3 = 432` |
| 3D dedicated periodic (`container_periodic`, `container_periodic_poly`) | `2 lattice types × 3 × 3 × 2 × 3 = 108` |
| **Total** | **756** |

## Local evidence (Linux x86-64)

Both integrated source profiles were tested locally with GCC `-O2` and Clang `-O3`. The full test runner, including Stages 1–3, returned exit code 0 in all four runs. This indicates that the controls behaved as required and any large-radius differences were explicitly recorded, **not that every test case passed**.

| Source version | 2D failing / 216 | 3D failing / 540 | Small-radius or standard controls failing |
|---|---:|---:|---:|
| Original Voro++ master | 66 | 174 | 0 |
| Existing `candidate-01` | 0 | 0 | 0 |

Every original failure in this matrix occurs with equal radii `10⁸`; the small-radius and ordinary cases pass. For the original, the 2D failures involve 1-, 2- or 3-site partially/fully periodic diagrams and 2-/3-site bounded diagrams. The 3D failures occur in the analogous rectangular masks **and** in the orthogonal and skew dedicated periodic containers. The exact failure count is specific to this deterministic input matrix, not a probability estimate or a universal scale threshold.

The existing `candidate-01` only changes `r_scale()`. Its complete success in these **equal-radius construction** cases does *not* repair the Stage 1 near-equal-radius arithmetic, the Stage 1 `r_scale_check()` decision failure, or the Stage 2 `find_voronoi_cell()` owner errors. Those remain intentionally separate diagnostic tests.

## Run and outputs

```bash
bash ./run.sh baseline
bash ./run.sh candidate
```

Each profile writes `output/<profile>/2d_coverage.txt` and `3d_coverage.txt` with every case's periodic flags, cell count, block count, insertion order, status and maximum deviation. The GitHub Actions workflow automatically captures the same outputs for its configured platforms.

The runner treats original large-radius discrepancies as informative (exit 1 from the coverage executable). A standard or small-radius control failure causes exit 2 and fails the run; any A6–A7 failure under the currently evaluated candidate also fails its run. This acceptance criterion is **limited to this equal-radius matrix**.

To avoid recompiling Voro++ eight times per profile, `run.sh` now builds each original native translation unit once, then links the standalone C++ programs against the corresponding object file. This does not change Voro++ itself.

## What is *not* proved yet

- Numerical correctness for **unequal** or extremely large genuine squared-radius differences.
- Correct `find_voronoi_cell()` behavior with partial or full periodicity.
- Correct identity and shift of individual periodic-image faces.
- Absence of errors for arbitrary multi-site geometry, degeneracies, or randomized inputs.
- Compatibility of the harness with Windows MSVC (not part of this stage).

These gaps form the next test-design decisions. The root README remains a provisional overview until the broader investigation is checked and its explanation has been reviewed in several iterations.
