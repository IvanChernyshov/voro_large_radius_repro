# Stage 6 — unequal-radius, multi-site power-cell regression tests (A8)

**Purpose:** test the proposed `candidate-02` plane-arithmetic repair against genuinely unequal radii, including empty cells and periodic images, without changing the original Voro++ source or the patch under test. The goal is to determine its **tested scope**, not yet certify it as a general solution.

## New native C++11 tests

- `repro/unequal2d.cpp` — 168 cases: 7 radius scenarios × 4 rectangular periodicity masks × 3 block grids (1, 2, 4 per axis) × 2 particle insertion orders.
- `repro/unequal3d.cpp` — 420 cases: 336 with all 8 rectangular periodicity masks, plus 84 with the dedicated `container_periodic_poly` in orthogonal and genuinely skew lattices.

Every cell is compared **individually**, not just via the sum of areas or volumes. We check nonempty/empty classification, measures and cell-coordinate extents. Control cases must succeed in every source profile.

### Exact scenarios

The domain has side length 10. Sites use x=2,5,8 (or x=2,8 for two sites); every y and z coordinate is 5. The input radii are:

| Scenario | Number of sites | Radii | Role |
|---|---:|---|---|
| `moderate-3` | 3 | 3, 1, 2 | Ordinary-scale positive control |
| `adjacent-2` | 2 | R, nextUp(R) | Two-site separated power boundary |
| `adjacent-3` | 3 | nextUp(R), R, nextDown(R) | Three sites with nearly equal large radii |
| `ulps-3` | 3 | R+4 ULP, R, R−2 ULP | Distinct large radii; changed cell widths |
| `dominating-3` | 3 | R, 0, 0 | A dominating site and empty competitors |
| `empty-middle-3` | 3 | R, 0, R | Two surviving sites and a genuinely empty middle cell |
| `medium-weight-3` | 3 | 10000, 9999.999, 9999.998 | Finite, distinct-weight positive control |

Here R = 10⁸. The ULP values are constructed directly with `std::nextafter` for adjacent radii; no decimal approximation substitutes for the binary64 inputs.

## Independent references and their boundaries

**Rectangular 2D and 3D:** all sites share y (and z), so every power cell is the Cartesian product of a *one-dimensional weighted interval in x* and a full interval in the other dimensions. For each site and every relevant x-periodic image, we intersect analytically derived power half-lines. With site positions `xi` and `xj+kL`, the bisector is

```text
B = (xi + (xj + kL))/2 + (ri-rj)*(ri+rj)/(2*((xj + kL)-xi))
```

The reference uses `long double` where available (note: MSVC represents `long double` as binary64); the critical radius difference is always factored before a large square is formed. Images with x-shifts k from −2 to 2 are considered when x is periodic, and the site's own images bound its lifted cell. The tests compare each native area/volume and its x-extrema against this analytic construction. The expected total is 100 in 2D and 1000 in 3D.

**Dedicated periodic 3D (`container_periodic_poly`):** a separate exhaustive reference loops through *all* site copies from image indices −2..2 along every basis direction and applies the corresponding cutting planes directly to an initially generous `voronoicell`. This bypasses `radius_poly`, accelerated block search and pruning. The reference plane uses a factored difference of the **input radii**, `|d|² + (ri−rj)(ri+rj)`. The volume and x/y/z coordinate extrema are checked for each cell, with empty cells handled explicitly. The test also constructs references from image ranges −1..1 and −2..2, and **aborts** if their nonempty classification or volumes disagree (a finite-image convergence check).

Two lattice choices have covolume 1000:

- Orthogonal: `(bx,bxy,by,bxz,byz,bz) = (10,0,10,0,0,10)`.
- Skew: `(10,2.3,10,-1.7,3.1,10)`.

**Limitation:** the skew reference shares Voro++'s primitive polyhedron-cutting implementation; it independently bypasses *radius adjustment and accelerated candidate discovery*, not the low-level clipping algorithm itself. It is not an exact-arithmetic general-purpose oracle.

## Locally executed comparison

A local copy of the source files used for compilation was verified to match the pinned Voro++ master Git blobs for both `rad_option.hh` files, `voro++.cc`, `voro++_2d.cc` and the relevant compute/container source files. The existing isolated candidate patches were applied to separate copies; the authoritative GitHub `vendor/voro/` was not altered.

On Linux x86-64 with GCC `-O2 -std=c++11` and Clang `-O3 -std=c++11`:

| Source profile | 2D mismatches /168 | 3D mismatches /420 | Control failures |
|---|---:|---:|---:|
| `baseline` (unmodified Voro++) | 72 | 188 | 0 |
| `candidate` (candidate-01) | 72 | 180 | 0 |
| `candidate-02` | **0** | **0** | 0 |

The original may preserve the *total* domain measure while assigning incorrect individual cell measures or locations. Candidate-01's small improvement in a few 3D high-contrast periodic cases does not solve the adjacent-radius scenarios.

The local test source was uploaded to GitHub using **identical Git blob SHA-1 IDs**, not reconstructed by hand with potentially different inputs.

## Running and interpreting results

```bash
bash ./run.sh baseline
bash ./run.sh candidate
bash ./run.sh candidate-02
```

On Windows use `pwsh -File ./scripts/run-msvc.ps1 -Mode <profile>`. The resulting `output/<profile>/2d_unequal.txt` and `3d_unequal.txt` contain every case and an `A8` summary. Failed ordinary-scale controls, invalid references or abnormal exits always fail the run. Only `candidate-02` currently **requires zero A8 mismatches**; the previous profiles record mismatches as diagnostic evidence.

## Remaining limits

These experiments do **not** test arbitrary non-collinear configurations in ordinary rectangular containers, highly anisotropic cells, all possible weight ranges or numerical overflow/underflow. The factored product can still overflow for some finite input radii. Correct periodic-image neighbor IDs and the separate `find_voronoi_cell()` route are also outside the acceptance scope. Further testing and an independent review are needed before recommending any final change to Voro++.

**No upstream issue is submitted or planned at this stage.** The main project README remains provisional.
