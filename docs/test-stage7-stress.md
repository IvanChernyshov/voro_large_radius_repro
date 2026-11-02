# Stage 7 — A9 non-collinear / noncoplanar power-cell stress tests

This stage tests candidate-02 against irregular multi-site cells, after the aligned-site tests in [Stage 6](test-stage6-unequal.md).

## Motivation

The earlier A8 matrix places all sites on one axis, giving an especially strong analytic oracle, but exercising comparatively simple bisectors and cell topology. A9 deliberately tests irregular 2D and 3D power cells with multiple intersecting half-spaces, nontrivial face/vertex incidence, empty cells, periodic images, different block grids and different insertion orders. Each cell is checked separately because the total area or volume can be correct even when individual cells are wrong.

## Input generation and test count

All inputs are deterministic and built directly in standard C++11 using a specified 32-bit linear congruential recurrence (not `std::uniform_real_distribution`, which may vary across platforms). Four fixed seeds each generate nine distinct sites within `[1,9)` in every coordinate, with minimum site separation enforced. The coordinate sets have full geometric rank (rank 2 for 2D, rank 3 for 3D). The cell domain is `[0,10]^d`.

Each site set is evaluated in six radius patterns:

| Pattern | Radii | Purpose |
|---:|---|---|
| 0 | Small radii 1–5, nonuniform | Mandatory positive control |
| 1 | All sites at `R=10^8` | Gauge-invariant equal-radius case |
| 2 | `R + offsets × ULP(R)` | Nearby binary64 radii |
| 3 | `R + 3×offsets × ULP(R)` | Wider local radius spread |
| 4 | One `R+12 ULP` site, others `R-(3+i) ULP` | Dominance and potential empty cells |
| 5 | One radius `R`, other radii zero | Extreme contrast and empty cells |

ULP offsets are derived from `std::nextafter` on the binary64 input radii.

The matrix varies the block grid (1 and 3 blocks per axis) and insertion order (forward and reversed).

- 2D: 4 seeds × 6 radius patterns × 4 periodicity masks × 2 grids × 2 insertion orders = **384 configurations per source profile**.
- 3D: 4 seeds × 6 patterns × (8 rectangular periodicity masks + orthogonal and genuinely skew dedicated periodic containers) × 2 grids × 2 orders = **960 configurations per profile**.

## Reference computation

**2D:** `repro/stress2d.cpp` contains an independent long-double polygon clipping implementation. It starts with the bounded or self-image-limited rectangle and applies *every* weighted bisector against all nearby site copies, using the factored difference `(r_i-r_j)(r_i+r_j)`. A direct shoelace formula calculates polygon area and centroid; vertex extrema give the axis-aligned bounding box. This oracle does not invoke the Voro++ clipping algorithms, search/block logic or `radius_poly`.

**3D:** `repro/stress3d.cpp` enumerates all nearby periodic image/site planes directly and clips an initial polyhedron using the **low-level Voro++ `voronoicell_neighbor::nplane` primitive**. It bypasses `radius_poly`, `container_poly::compute_cell`, `container_periodic_poly::compute_cell` and accelerated block pruning, but **shares the primitive 3D polyhedron clipping algorithm**. It is therefore an independent *search/radius-arithmetic reference*, not a fully independent polyhedral geometry implementation.

For 2D and 3D, the oracle is recomputed with image ranges `[-1,1]` and `[-2,2]` along periodic lattice axes. The reference aborts with exit code 2 if the resulting cell geometry does not converge. It also checks that reference areas sum to 100 or volumes sum to 1000. In 3D the skew periodic lattice is

```text
(bx,bxy,by,bxz,byz,bz) = (10,2.3,10,-1.7,3.1,10).
```

The comparison records for **each native cell** its empty/nonempty status, area/volume, centroid, and coordinate extrema. Per-quantity tolerances are `1e-5` in 2D, `2e-5` in 3D; the total-area/volume checks use `2e-5` and `5e-4`, respectively. The absolute tolerances are chosen for the 10-unit domain used in A9.

## Locally verified results

The source versions used for these runs have the exact original Voro++ `src/rad_option.hh`, `2d/src/rad_option.hh`, translation units and compute sources, checked against the pinned Git blob hashes. Candidate-01 and candidate-02 were applied independently, not cumulatively.

Linux x86-64; both GCC `-O2 -std=c++11` and Clang `-O3 -std=c++11`:

| Source profile | 2D failures /384 | 3D failures /960 | Positive-control failures |
|---|---:|---:|---:|
| Original Voro++ | **232** | **646** | 0 |
| Candidate-01 | **168** | **472** | 0 |
| Candidate-02 | **0** | **0** | 0 |

Each failure count describes the number of **configurations** with at least one incorrect individual cell or total area/volume, not the number of wrong faces or generators. The coordinates and radii are fixed across profiles.

The two test source files were uploaded under Git blob SHA-1s matching the locally compiled files exactly: `stress2d.cpp` = `7d2a9d1f66a4f2b8da8083c1b8ffba80399b50ed`; `stress3d.cpp` = `8c12e141bce992086e883383e881105784f0bd40`.

## Independent GitHub Actions results

The exact GitHub-committed test sources were executed in [workflow run 41316575492](https://github.com/IvanChernyshov/voro_large_radius_repro/actions/runs/41316575492) at commit `191d552f8b5f0598e6683ba0abf1c4186ba60fa9`. The table below covers the four platform profiles completed in that run. The combined-patch CI later added macOS ARM64 coverage; see [Stage 10](test-stage10-combined.md).

| Platform / compiler | Original wrong 2D / 3D | Candidate-01 wrong 2D / 3D | Candidate-02 wrong 2D / 3D |
|---|---|---|---|
| Ubuntu, GCC `-O2` | 232 / 644 | 168 / 476 | **0 / 0** |
| Ubuntu, Clang `-O3` | 232 / 646 | 168 / 472 | **0 / 0** |
| macOS, Clang `-O2` | 232 / 644 | 168 / 476 | **0 / 0** |
| Windows x64, MSVC `/O2` | 232 / 646 | 168 / 472 | **0 / 0** |

All moderate-radius positive controls passed, and the independently checked reference volumes and image-range convergence were valid. Each 2D fraction has denominator 384 configurations and each 3D fraction denominator 960 configurations, per profile and platform.

The precise **number of erroneous 3D configurations varies slightly with compiler and platform**. They are observed counts for this matrix, not a fixed failure rate. The candidate-02 result is the stricter check: any mismatch fails that profile.

The original and candidate-01 jobs retain the numerical discrepancies as diagnostic results. The candidate-02 job requires all cases to match.

## How to run and interpret

```bash
bash ./run.sh baseline
bash ./run.sh candidate      # candidate-01
bash ./run.sh candidate-02
```

Windows MSVC uses `./scripts/run-msvc.ps1 -Mode <profile>` from Developer PowerShell. The output logs are `output/<profile>/2d_stress.txt` and `3d_stress.txt` and are collected by GitHub Actions.

- Exit 0: all A9 cases passed.
- Exit 1: only the high-radius regimes differ; expected **diagnostic evidence** in the original and candidate-01 profiles.
- Exit 2: an ordinary-radius control or an oracle consistency check failed; fatal for **every** profile.

**For candidate-02, any nonzero exit is fatal.** The existing A1–A8 checks remain in the same runner and retain their individual acceptance criteria.

## Scope and subsequent tests

A9 establishes construction accuracy for a finite set of irregular geometries. The owner-search path remains separate and is addressed in [Stage 8](test-stage8-candidate03.md). The extreme-value and near-tie tests are in [Stage 9](test-stage9-extremes.md). MSVC's `long double` has binary64 precision, so its reference calculation does not gain the extended precision available on some Unix toolchains. The original source snapshots remain pinned throughout.
