# Working draft: Voro++ large-radius cancellation

This draft collects the reproducible observations for a possible
upstream issue. The wording and scope remain open for review.

## Suggested title

Large radii cause incorrect radical cell geometry and power-site lookup in 2D/3D

## Observed behavior

In the pinned Voro++ `master` (`b0dac575`) and `dev` (`61099f68`)
revisions, the native radical tessellation can give incorrect results
when radii are large compared with the site separations.

For example, in a nonperiodic unit square, consider two sites at
`(0.25,0.5)` and `(0.75,0.5)`, each with radius `1e8`.
Equal radii must give the ordinary bisector `x=0.5` and cell areas
`0.5,0.5`. Both unmodified versions instead return `0.25,0.25`.
The analogous 3D unit-cube example gives volumes `0.25,0.25`.

In the same configurations, `find_voronoi_cell` at `x=0.74`
returns site `0` (`x=0.25`) instead of site `1` (`x=0.75`).
Small-radius controls pass.

## Reproduce

```bash
git clone https://github.com/IvanChernyshov/voro_large_radius_repro.git
cd voro_large_radius_repro
bash ./scripts/reproduce.sh        # master
bash ./scripts/reproduce-dev.sh    # dev
```

These C++11 runners compare the original and experimentally patched
source, reporting the expected four native errors and four corrected
results. They apply patches to temporary copies of the pinned
upstream sources. Additional multi-site, periodic and cross-platform
tests are collected in the same repository.

## Numerical explanation

Two separate routes lose precision:

1. Cell construction uses `rs + r_i² - r_j²` with separately
   rounded squared radii. At equal radii near `1e8`, the geometric
   term can be rounded away. The tested correction replaces the
   difference with `(r_i-r_j)*(r_i+r_j)` in both `r_scale` and
   `r_scale_check`.
2. Owner search compares `||x-p_i||²-r_i²`, then uses
   `r_max_add` in the block-pruning threshold. The tested correction
   instead compares `||x-p_i||²+(R-r_i)*(R+r_i)`, where
   `R=max_i r_i` for nonnegative radii, and adjusts
   `r_max_add` to match the shifted score.

The two corrections are supplied as a combined patch, adapted
separately for `master` and `dev`. They passed the documented
finite test matrices, including the expanded `dev` dedicated
triclinic checks.

## Evidence and limits

[Current cross-platform CI run](https://github.com/IvanChernyshov/voro_large_radius_repro/actions/runs/41401868846),
[combined `master` report](test-stage10-combined.md), and
[`dev` comparison](dev-branch.md) include results and source
revisions. A9 checks individual cells against direct clipping;
A10 checks owners against enumerated power distances and images.

The owner-search reasoning assumes nonnegative radii. Intermediate
overflow (e.g. at `1e308`), negative-radius inputs and
near-degenerate comparisons are outside the established correction
range; the corresponding observations are documented separately.

## Question for the maintainer

Would a factorized radius difference in the cell-construction path
and a consistently shifted owner-search score be appropriate for
Voro++? The repository contains the minimal reproductions, separate
patches for both source layouts, and the regression results needed
to review the change.
