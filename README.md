# Numerical cancellation in Voro++ radical tessellations

**Related issue:** [Voro++ #46](https://github.com/chr1shr/voro/issues/46)

While adding weight-based power-diagram calculations to pyvoro2, we found that
Voro++ can produce incorrect cells and nearest-power-site queries when the
radii are large compared with the site separations. This repository isolates
the failures in native C++, examines their numerical causes, and tests small
corrections against pinned Voro++ `master` and `dev` source trees.

## Reproducing the failures

For sites `p_i` and radii `r_i`, the power distance is

```text
pi_i(x) = ||x - p_i||² - r_i².
```

A common shift of all squared radii leaves the power diagram unchanged. In
particular, equal radii of any magnitude give the ordinary Voronoi diagram.

Consider two generators in a unit square:

```text
p0 = (0.25, 0.5)      r0 = 1e8
p1 = (0.75, 0.5)      r1 = 1e8
```

Their separator is `x = 0.5`, so each cell has area `0.5`. The unmodified
Voro++ returns areas `0.25` and `0.25`, leaving a gap. The corresponding
unit-cube example, with both generators at `z = 0.5`, gives the same
incorrect volumes.

There is a second failure in the same example: `find_voronoi_cell` queried
at `x = 0.74` returns generator `0` at `x = 0.25`, although generator
`1` at `x = 0.75` is the unique correct owner. This occurs in both 2D
and 3D, independently of how cell volumes are measured.

On Linux or macOS, reproduce the original results and test the combined
corrections with a C++11 compiler, Bash and `patch`:

```bash
bash ./scripts/reproduce.sh             # upstream master
bash ./scripts/reproduce-dev.sh         # upstream dev
```

Each script checks four expected failures (cell construction and owner search
in 2D and 3D) against the original sources, then requires all four checks
to pass with the corrected sources. The source trees in `vendor/` are kept
intact; patches are applied to temporary copies.

## Where precision is lost

Two numerical expressions are responsible for the demonstrated errors.

### Constructing power cells

`r_scale` computes the plane displacement as `(rs + ri*ri) - rj*rj`.
At `ri = rj = 1e8`, adding the small squared displacement `rs` to
`1e16` can round away `rs` before the equal squares cancel.
`r_scale_check` instead evaluates `rs + (ri*ri - rj*rj)`; it does
not have that same addition-order problem for equal radii. Both paths
can nevertheless lose differences between *nearby* radii when the
squares are rounded independently.

The construction correction (candidate-02) retains the unsquared current
radius and uses the factored difference in `r_scale` and `r_scale_check`:

```cpp
rs + (ri-rj)*(ri+rj)
```

This also resolves the tested cases with adjacent representable
`double` radii near `1e8`.

### Finding the owner of a point

The native `r_current_sub` path compares
`||x-p_i||² - r_i²`. When the squared radii dominate the geometric
distances, the differences that determine the nearest power site can
disappear under rounding.

For nonnegative radii and `R = max_i r_i`, candidate-03 instead compares

```text
s_i(x) = ||x-p_i||² + (R-r_i)*(R+r_i)
       = pi_i(x) + R²                         (in exact arithmetic).
```

In exact arithmetic the shift preserves the minimizer. Moreover,
`s_i(x) >= ||x-p_i||²`, so a block's lower bound on squared Euclidean
distance remains valid for owner search. The accompanying change makes
`r_max_add` an identity to keep scoring and pruning consistent.

This is **not** a floating-point guarantee: a large radius at a remote
site can make the shifted scores of nearby, small-radius sites round to
a tie. In a verified three-site counterexample, original Voro++ locates
the correct site but candidate-03 and the combined patch return the
wrong one, in 2D and 3D on both branches. See the
[owner-shift diagnostic](docs/test-stage10-combined.md#global-shift-owner-search-regression)
and its standalone probe.

## Experimental patches

The combined candidate applies both corrections to the same clean source
snapshot:

| Upstream revision | Source directory | Patch |
|---|---|---|
| `master` (`b0dac575`) | `vendor/voro/` | [`patches/candidate-02-plus-03.patch`](patches/candidate-02-plus-03.patch) |
| `dev` (`61099f68`) | `vendor/voro-dev/` | [`patches/dev/candidate-02-plus-03.patch`](patches/dev/candidate-02-plus-03.patch) |

The `master` patch changes the 2D and 3D `rad_option.hh` headers. In
`dev`, both dimensions use the reorganized `src/` tree; the patch also
passes the unsquared current radius through the local variables in
`v_compute_2d.cc` and `v_compute_3d.cc`. It does not add shared
mutable radius state.

The earlier independent patches (`candidate-01`, `candidate-02`, and
`candidate-03`) remain in `patches/` to show which arithmetic change
affects each operation.

## Tests and results

The regression programs compare individual cells with analytical or
direct-clipping references, and owner queries with direct enumeration of
power distances and periodic images. The 2D irregular-cell reference
implements polygon clipping independently; the 3D reference enumerates
cutting planes without the Voro++ block search but uses its low-level
polyhedron clipper.

| Test family | Combined-patch result |
|---|---|
| A1–A3: plane arithmetic, including adjacent radii | Pass in 2D/3D |
| A4–A7: basic owner and periodic-cell checks (`master`) | Pass |
| A8: unequal-radius cell geometry (`master`) | 588/588 |
| A9: irregular cell geometry (`master`) | 1,344/1,344 |
| A10: owner and periodic-image queries (`master`) | 75,168/75,168 |
| A11–A12: selected larger-radius and domain cases (`master`) | Pass within the defined test range |
| A9: irregular cells (`dev`) | 384/384 in 2D; 960/960 in 3D, including 192 dedicated periodic cases |
| A10: owner queries (`dev`) | 36,288/36,288 in 2D; 38,880/38,880 in 3D |
| Dedicated triclinic cell checks (`dev`) | 40/40, versus 24/40 with original `dev` |
| Four-thread OpenMP comparison (`dev`) | No mismatches in the tested cell and owner queries |
| Additional owner-shift probe (both branches) | Original correct; combined owner-search patch gives wrong owner in the counterexample (diagnostic, not a pass claim) |

The full `dev` A9 matrix passes with GCC and Clang on Linux x86-64 and
with Apple Clang on macOS ARM64 and Intel. The `master` tests also pass
with Windows x64/MSVC. Unmodified `dev` fails to compile under MSVC
2022 because of an unrelated `iterator` declaration conflict (C2990);
the `dev` patch has therefore been tested on Linux and macOS.

[CI run 41401868846](https://github.com/IvanChernyshov/voro_large_radius_repro/actions/runs/41401868846)
contains the completed cross-platform matrix. For detailed inputs,
tolerances and reference calculations, see the
[combined `master` report](docs/test-stage10-combined.md) and
[`dev` report](docs/dev-branch.md).

### Further regression commands

```bash
bash ./run.sh combined-02-03
bash ./scripts/reproduce-dev.sh extended
bash ./scripts/run-dev-openmp.sh
bash ./scripts/probe-dev-triclinic.sh
bash ./scripts/probe-owner-shift.sh
```

The final command records additional diagnostic cases; baseline
mismatches there are expected. On Windows, run
`pwsh -File ./scripts/run-msvc.ps1 -Mode combined-02-03` from a
Visual Studio Developer PowerShell. `CXX` and `CXXFLAGS` select the
compiler and options for the Bash runners.

## Numerical limits

The tests establish the corrections for specific finite input families,
rather than all representable radii and geometries. In particular:

- The owner-search shift assumes nonnegative radii. Negative-radius
  inputs can give wrong owners with the proposed patch; the supported
  semantics of signed radii have not been established.
- Even for nonnegative radii, candidate-03 can **introduce** owner-search
  errors when a remote large-radius site sets the global shift and
  two smaller-radius sites have distinguishable unshifted scores. The
  combined patch inherits this regression. Its previously passing A10/A11
  matrices do not cover this configuration.
- At `r = 1e308`, finite input radii generate overflowing intermediate
  sums or squares, and the corrected cell construction still fails.
  Tests at `1e8`, `1e154` and `1e200` pass for the specified
  equal-radius geometry.
- Near ties, extreme coordinate scales and the remaining radial
  pruning predicates have separate numerical limitations. The
  [pruning arithmetic probes](docs/test-stage11-pruning-scope.md)
  distinguish scalar rounding effects from demonstrated cell failures.

## Sources and repository files

The `vendor/voro/` and `vendor/voro-dev/` directories preserve exact,
unmodified upstream snapshots; see [source provenance](PROVENANCE.md).
The `repro/` directories contain the C++ tests, `scripts/` and `run.sh`
run them, and `docs/` contains the staged experimental records and
a [working upstream issue draft](docs/issue-draft.md).

Original test code and documentation are MIT licensed; the vendored
Voro++ sources retain their upstream license. Details are in
[LICENSING.md](LICENSING.md).
