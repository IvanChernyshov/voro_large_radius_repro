# Draft for Voro++ issue — NOT POSTED

## Title

Large equal radii cause missing radical Voronoi cell volume in 2D and 3D (`r_scale` cancellation)

## Summary

In the original Voro++ `master` commit `b0dac575`, finite and representable radii can produce **incorrect native power-diagram geometry**, even for two sites in a nonperiodic unit box and with exactly equal radii. The failure occurs directly in native Voro++ and is reproduced using standalone C++.

The repository provides a full unchanged Voro++ source snapshot, minimal native reproducers and a CI matrix.

## Minimal inputs

**2D:** unit box, sites (0.25,0.5) and (0.75,0.5), both radii 10⁸, radical container. Expected: two halves of area 0.5 each, separated at x=0.5. Observed: only 0.5 total area and a 0.5-wide gap.

**3D:** analogous sites (0.25,0.5,0.5) and (0.75,0.5,0.5) in a unit cube. Expected two cells of volume 0.5; observed total ~0.5.

**Periodic sanity check:** a single site of radius 10⁸ in a unit fully periodic square/cube must own the whole fundamental domain; original native computation instead reports ~0.5 for one block per axis. The effect also depends on block grid and coordinate/domain scale.

With both radii set to 1.0, all these checks pass.

## Reproduction commands

```bash
git clone https://github.com/IvanChernyshov/voro_large_radius_repro
cd voro_large_radius_repro
bash ./run.sh baseline
bash ./run.sh candidate
```

The first command runs unmodified Voro++ and records expected high-radius failures. The second runs an **experimental**, separate arithmetic candidate, never modifying the original source.

## Source-level hypothesis

In `radius_poly::r_scale` (2D and 3D), the original expression computes

```cpp
rs + r_rad - r_other*r_other
```

with r_rad = r_current². When both radii equal 10⁸, the exact radius difference is zero but the intermediate `rs + 10^16` loses a geometric term of order one in binary64 arithmetic. In this example, a displacement of 0.25 becomes 0. A candidate that evaluates the radius difference **before** adding rs recovers the expected geometry on the test matrix.

This explains a concrete failure mechanism, but it is **not yet a proof** that the candidate covers all numerical paths or huge, genuine squared-radius differences.

## Request

Could the original Voro++ radical plane-displacement arithmetic be reviewed for cancellation at very large common radii, and for any other affected paths? The linked repository provides standalone C++ evidence and a separated candidate. The objective is to make the expected arithmetic invariant visible and assess a safe, minimal repair without introducing a dependence on newer C++ language standards.

## Environment and evidence

Source version: original master SHA `b0dac575a47af0f90b5b100e6dc199a493c7cb83`. See GitHub Actions matrix and artifacts for exact runner/compiler versions and per-case measurements.

**Do not submit this draft until the candidate and original source have also been evaluated in the GitHub CI matrix.**
