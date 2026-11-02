# Voro++ large-radius reproducibility study

Standalone native C++ investigation of **two numerical failures** in Voro++
radical (power) tessellations when absolute site radii are very large relative
to the geometry scale. It reproduces both failures in **2D and 3D**, in pinned
upstream `master` and `dev` snapshots, and tests an isolated experimental
correction for each upstream architecture.

This is **not** a Python/pyvoro2 bug report: the incorrect results occur in
native Voro++. No upstream issue or pull request is submitted by this repository.

## The failure: two independently affected operations

For sites `p_i` with radii `r_i`, a power diagram minimizes

```text
power_i(x) = ||x - p_i||² - r_i² .
```

Adding the same constant to **every squared radius** cannot change the
diagram. In particular, two equal radii must give the ordinary unweighted
bisector, regardless of their common magnitude.

**Minimal 2D example:** place `p0=(0.25,0.5)` and `p1=(0.75,0.5)` in
`[0,1]²`, both with `r=1e8`. The correct bisector is `x=0.5`, and each
cell has area `0.5`. The unmodified Voro++ returns areas `0.25, 0.25`,
leaving half the domain uncovered. The corresponding **3D** example uses
`z=0.5` and a unit cube; its computed cell volumes are likewise
`0.25, 0.25` instead of `0.5, 0.5`.

**Separate owner-search example:** `find_voronoi_cell` queried at
`(0.74,0.5)` in 2D or `(0.74,0.5,0.5)` in 3D should return site ID
`1` at `x=0.75`. The unmodified native implementations instead return
ID `0`. This is **not** just a problem extracting volumes from cells:
the accelerated location path also loses geometrically meaningful
differences of distances.

Small-radius controls behave correctly. Additional tests cover a
single site in a periodic domain, unequal radii, non-collinear or
non-coplanar site configurations, partial/full periodicity, insertion
order and spatial grids. See [findings](docs/findings.md) and the
[full test history](docs/test-stage10-combined.md).

## Numerical cause and experimental corrections

The two failures arise in distinct native code paths:

| Native operation | Problematic arithmetic | Proposed approach |
|---|---|---|
| `compute_cell` | `rs + r_i² - r_j²`, evaluated through separately rounded squares | Factor the difference before adding `rs` |
| `find_voronoi_cell` | `||x-p_i||² - r_i²` loses geometric differences; later block thresholds cannot restore them | Compare common-shifted scores and update the matching pruning threshold |

**Cell construction — candidate-02.** The original code effectively uses

```cpp
rs + r_rad - r_other*r_other  // r_rad was computed as r_current*r_current
```

For equal `r_current = r_other = 1e8`, adding a small `rs` to `1e16`
first rounds away the small term. The candidate instead evaluates

```cpp
rs + (r_current-r_other)*(r_current+r_other)
```

in both `r_scale` and `r_scale_check`. Unlike simply reordering
already-squared values (historical candidate-01), this also preserves
useful differences for **nearby but unequal** large radii.

**Owner search — candidate-03.** For nonnegative radii with
`R = max(r_i)`, use

```text
shifted_i(x) = ||x-p_i||² + (R-r_i)*(R+r_i)
             = power_i(x) + R²                         (exact arithmetic).
```

The argmin is unchanged, while the factored addition avoids
subtracting two independently rounded large squares. Crucially,
`shifted_i(x) >= ||x-p_i||²`; the existing geometric lower bounds for
unvisited spatial blocks remain conservative with the new score.
Accordingly, `r_max_add` becomes an identity **together with** the
change to `r_current_sub`. Changing only the score would not justify
the original pruning comparisons.

The preferred experimental correction is **candidate-02 + candidate-03**,
not either partial patch on its own:

| Pinned upstream snapshot | Standalone combined patch |
|---|---|
| `master` — `vendor/voro/` | [`patches/candidate-02-plus-03.patch`](patches/candidate-02-plus-03.patch) |
| `dev` — `vendor/voro-dev/` | [`patches/dev/candidate-02-plus-03.patch`](patches/dev/candidate-02-plus-03.patch) |

The branches have different C++ architecture. The `master` patch
changes the two 2D/3D `rad_option.hh` headers. In `dev` the active
implementation combines the dimensions under `src/`; its patch
changes `src/rad_option.hh` and forwards an **unsquared caller-local**
radius through `src/v_compute_2d.cc` and `src/v_compute_3d.cc`.
It introduces no shared mutable current-radius field.

These are **proposed, experimentally validated numerical corrections
for the stated input classes**, not universal correctly-rounded power
predicates or an all-finite-input guarantee.

## Reproduce the two minimal failures

On Linux/macOS, requirements are a C++11 compiler, Bash and the
`patch` utility; Python is **not** required. The runners apply a patch
to a **temporary source copy** and leave both vendored trees intact.

```bash
# Pinned master: known incorrect original output, then corrected 2D/3D checks.
bash ./scripts/reproduce.sh

# Pinned dev: the same two native defects in the reorganized source.
bash ./scripts/reproduce-dev.sh
```

Each command asserts that the original produces **four specified
incorrect checks** (cell construction and owner search in 2D/3D),
and that the combined patch passes all four. A green baseline
reproducer therefore means the **expected error was reproduced**;
it does **not** mean the original native geometry was correct.

For broader regression matrices:

```bash
bash ./run.sh combined-02-03                  # master: A1–A12
bash ./scripts/reproduce-dev.sh extended      # dev: A1–A3, A9, A10
bash ./scripts/run-dev-openmp.sh              # dev: GCC/OpenMP concurrency
bash ./scripts/probe-dev-triclinic.sh         # dev: diagnostic-only, not a pass gate
```

Choose compilers/optimization with `CXX` and `CXXFLAGS`, e.g.
`CXX=clang++ CXXFLAGS='-O3 -std=c++11' bash ./run.sh combined-02-03`.
The `master` snapshot also has a Windows MSVC runner:
`./scripts/run-msvc.ps1 -Mode combined-02-03` from a Visual Studio
Developer PowerShell. Historical independent modes
`candidate` (= candidate-01), `candidate-02`, and `candidate-03`
remain available for investigation, but are **not** the recommended
end-to-end correction.

## Validation and coverage

Tests check the **individual** cell measures, geometry extents and,
for irregular sites, centroids/activation as well as domain-measure
conservation. Owner queries use an enumerated shifted-power
reference, including periodic-image coordinates. The 2D A9 reference
clips polygons independently in `long double`; 3D A9 enumerates
planes independently of Voro++'s search/block pruning but reuses its
low-level 3D clipping primitive. These oracles have finite, explicitly
tested periodic-image ranges and do not certify every lattice.

| Regression family | Combined result on tested configurations |
|---|---|
| A1–A3, factorized plane arithmetic and analytical separators | Pass, both dimensions |
| A4–A7, location, periodicity and domain/grid controls (`master`) | Pass |
| A8, unequal-radius construction (`master`) | 588/588 |
| A9, irregular-site construction (`master`) | 1,344/1,344 |
| A10, accelerated owner queries (`master`) | 75,168/75,168 |
| A11–A12, selected nonnegative / extreme cases (`master`) | Pass within documented acceptance scope |
| A9, 2D rectangular / 3D rectangular + dedicated triclinic (`dev`) | 384/384 and 960/960 on Linux GCC |
| A10, 2D and 3D owner queries (`dev`, including triclinic owner search) | 36,288/36,288 and 38,880/38,880 |
| Dedicated triclinic 3D cell construction (`dev`, isolated primary-site probe) | 40/40 corrected; original 24/40 |
| OpenMP concurrency (`dev`, four threads) | No mismatches in 2D/3D cell and owner checks |

The selected `dev` regression matrices pass on Linux x86-64
(GCC `-O2`, Clang `-O3`) and macOS ARM64/Intel (Apple Clang).
The `master` validation additionally covers Windows x64/MSVC.
The **unmodified dev snapshot** fails to compile under MSVC 2022
due to an unrelated `iterator` declaration error (`C2990`), so
Windows/MSVC is not claimed for `dev`.

CI evidence: [current completed run](https://github.com/IvanChernyshov/voro_large_radius_repro/actions/runs/41397139610);
[combined master analysis](docs/test-stage10-combined.md);
[dev adaptation and precise coverage](docs/dev-branch.md).
A test matrix with no reported mismatches is finite evidence, **not**
a formal correctness guarantee for all input geometries.

## Scope and outstanding limitations

- The shifted-score/pruning argument assumes **finite, nonnegative
  input radii**; the original API's handling of negative radii is not
  established as a supported contract, and some negative-radius
  cases fail.
- Finite radii can still overflow in intermediate products: for
  example `1e308` is outside the demonstrated correction's
  successful cell-construction range. Exact and nearly tied power
  scores, and near-threshold pruning, require separate care.
- Other radial pruning expressions (`r_ctest`, `r_prime`,
  `r_cutoff`) are unchanged. [Scope-only arithmetic probes](docs/test-stage11-pruning-scope.md)
  are **not** additional geometric acceptance guarantees.
- Dedicated `dev` triclinic **cell construction** passes 40/40
  independent-reference checks with the corrected combined patch
  (24 ordinary-radius controls and 16 large-radius cases).
  The unpatched source passes the 24 controls but fails the 16
  large-radius comparisons. An earlier harness used `iterator != end()`
  while `compute_cell` dynamically created image particles; it
  accidentally traversed generated image blocks and produced
  unrelated internal index errors. The corrected test captures only
  primary-site indices before computing cells. This finite test is
  **not** a universal triclinic correctness guarantee. The corrected
  nine-site A9 also passes 960/960 configurations on Linux GCC, including
  192 dedicated periodic cases. See [`docs/dev-branch.md`](docs/dev-branch.md).

## Repository layout and provenance

- `vendor/voro/`, `vendor/voro-dev/`: complete, **unmodified**
  upstream source snapshots, each pinned and Git-tree verified.
- `patches/`, `patches/dev/`: isolated candidate diffs (never
  edits applied to the source snapshots).
- `repro/`, `repro/dev/`: standalone native C++ reproductions and
  finite-oracle regression programs.
- `run.sh`, `scripts/`, `.github/workflows/`: runners and CI;
  generated measurements are archived as CI artifacts.
- `docs/`: detailed experimental stages, numerical limits,
  dev-specific evidence and an **unposted, outdated**
  [upstream issue draft](docs/issue-draft.md).

Upstream origins, exact commit/tree IDs and snapshot validation:
[`PROVENANCE.md`](PROVENANCE.md).
Standalone study code/documentation are MIT licensed; preserved
upstream Voro++ source and upstream-derived patch context retain
their corresponding original license terms. See
[`LICENSING.md`](LICENSING.md).
