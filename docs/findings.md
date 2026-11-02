# Initial numerical findings

This record covers the first equal-radius construction reproductions in
Voro++ `master` and the initial arithmetic candidate. Later tests found a
separate owner-search failure and led to the combined patch described in
the [README](../README.md).

## Analytical reference

For two sites `p0=(L/4,L/2)` and `p1=(3L/4,L/2)` (with
`z=L/2` in 3D), equal radii cancel from their power-bisector equation.
Their separator is `x=L/2`, and each cell occupies half the
`L × L` square or cube. A single site in a fully periodic cell has
area `L²` or volume `L³`.

These exact geometric relations supply direct checks on the native cell
measures and separating planes.

## First reproduction

The original Voro++ `master` sources were compiled on Linux x86-64
with GCC `-O2` and Clang `-O3`. The tested code was subsequently
pinned in `vendor/voro/` and matched against the original Git tree.

| Radius | Box side `L` | Blocks per axis | Original 2D | Original 3D | Candidate-01 |
|---:|---:|---:|---|---|---|
| 1 | 1 | 1 | Pass | Pass | Pass |
| 10⁶ | 1 | 1 | Pass | Pass | Pass |
| 10⁷ | 1 | 1 | Pass | Pass | Pass |
| 10⁸ | 1 | 1 | Fail | Fail | Pass |
| 10⁸ | 1 | 2 | Fail | Fail | Pass |
| 10⁸ | 100 | 1 | Pass | Pass | Pass |

With two sites, `r=1e8`, `L=1` and one block per axis, the original
cells cover only `0.5` of the domain, leaving a gap of width `0.5`
along x. In a fully periodic one-site case, the original code gives
about half the expected measure with one block; some two-block
configurations give zero measure.

## Arithmetic explanation and later correction

The original plane displacement in `rad_option.hh` is equivalent to

```cpp
rs + r_rad - rj*rj       // r_rad = ri*ri
```

At `ri=rj=1e8`, the small `rs` is lost when added to `1e16`,
before the large squares cancel. The first candidate (candidate-01)
reordered the expression:

```cpp
rs + (r_rad-rj*rj)
```

It fixes these equal-radius cases. Tests with adjacent representable
large radii showed that subtracting already-rounded squares remains
inaccurate. Candidate-02 instead uses
`rs + (ri-rj)*(ri+rj)` and passes the subsequent unequal-radius and
irregular-cell construction tests. Candidate-03 addresses the separate
`find_voronoi_cell` path; the combined patch applies both corrections.

The tests and their historical results are documented in
[Stages 1–3](test-stage1.md), [candidate-02](test-stage5-candidate02.md),
[combined validation](test-stage10-combined.md), and
[the `dev` comparison](dev-branch.md).

In periodic containers, negative adjacent IDs may denote self-images
rather than incorrect neighbor information; the construction checks
above use cell measures and geometry instead.
