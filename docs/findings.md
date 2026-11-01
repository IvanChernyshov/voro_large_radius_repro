# Numerical findings — working investigation

## Mathematical oracle

For two sites separated along x, `p0=(L/4,L/2)` and `p1=(3L/4,L/2)` (and z=L/2 in 3D), with equal radii r, the squared-radius terms cancel **exactly** in the radical bisector equation. The boundary is `x=L/2`; each cell has half the measure of the L-by-L (or L-by-L-by-L) domain. This is true for *any common radius* in exact arithmetic.

For one site in a fully periodic L-sized fundamental domain, every translated copy has the same radius. Its unique fundamental cell has measure L² in 2D and L³ in 3D.

These checks do not rely on another tessellation implementation.

## Observed reproduction

A native C++/Bash matrix was compiled and executed locally on Linux x86-64 with GCC `-O2` and Clang `-O3`. The initial local source copy had the key `src/rad_option.hh` / `2d/src/rad_option.hh` Git blobs identical to the pinned original master. The full GitHub source directory in this project has subsequently been checked for **complete tree identity** with original master.

| r | L | block grid | Original 2D | Original 3D | Candidate 2D/3D |
|---:|---:|---|---|---|---|
| 1 | 1 | 1 | pass | pass | pass |
| 10⁶ | 1 | 1 | pass | pass | pass |
| 10⁷ | 1 | 1 | pass | pass | pass |
| 10⁸ | 1 | 1 | **fail** | **fail** | pass |
| 10⁸ | 1 | 2 per axis | **fail** | **fail** | pass |
| 10⁸ | 100 | 1 | pass | pass | pass |

At r=10⁸, L=1, and one block per axis, the **original** equal-radius two-site cells together occupy only **0.5** of the domain, with a geometric x-gap of **0.5**; the one-site periodic domain also occupies only ~0.5. With two blocks per axis, the one-site periodic container may report zero measure. The candidate patch restored all tested measures and x extents on both compilers.

These are tests of selected geometries, **not** a claim that the candidate fixes every large power-weight range. GitHub Actions runs the exact GitHub-committed source and archives full logs for independent platform evidence.

## Focused source explanation

The original radical plane-displacement expression in `src/rad_option.hh` and `2d/src/rad_option.hh` has the sequential form:

```cpp
return rs + r_rad - r_other*r_other;
```

When r_rad and r_other² are about 10¹⁶ and rs is about 0.25, binary64 evaluation of `(rs + r_rad)` discards the small rs before subtracting r_other². Even when both radii are **identical**, the computed displacement can become zero instead of 0.25.

The current experimental candidate groups the radius difference first:

```cpp
return rs + (r_rad - r_other*r_other);
```

This is algebraically identical and preserves rs for equal radii. Other numerical paths such as radial pruning, genuine large radius differences, overflows, and degeneracies require separate testing. A successful result for this candidate is not a general robust-arithmetic certificate.

## Follow-up experiments

1. Find the failure threshold as a function of absolute radius, domain scale, and compiler settings.
2. Separate huge equal radii from near-equal radii and genuine large squared-radius differences.
3. Inspect other arithmetic combinations and verify that the candidate cannot miss required cuts in less simple geometries.
4. Check original vs candidate on macOS and Linux with recorded compiler/toolchain details.
5. Once these results are stable, finalize a focused Voro++ issue with links to actual CI runs.

Negative neighbor IDs in periodic containers may represent self-images, not necessarily a defect; tests here rely on analytical measures and locations instead.
