# Voro++ large-radius reproducibility study

A standalone **C++/Bash** investigation of numerical failures in Voro++ radical (power) tessellations when absolute squared radii become very large relative to squared geometric distances.

This repository is independent of Python packages and wrappers. It contains a pinned, **unmodified** copy of the original Voro++ `master` sources, small native 2D/3D reproducers, and a separated experimental fix.

## Source version

The original Voro++ source tree in [`vendor/voro/`](vendor/voro/) corresponds to [`chr1shr/voro` `master` at `b0dac575a47af0f90b5b100e6dc199a493c7cb83`](https://github.com/chr1shr/voro/commit/b0dac575a47af0f90b5b100e6dc199a493c7cb83) (2026-03-04). It is **not edited** by this project; candidate fixes are stored as patches and applied to temporary copies during testing.

See [`PROVENANCE.md`](PROVENANCE.md) for snapshot and verification details.

## Minimal geometric counterexamples

* **Equal-radius pair in a unit box (2D/3D):** sites at (x=0.25) and (x=0.75), with equal radii `r=1e8`. Their power bisector must be (x=0.5), and each cell must have exactly half the domain area/volume.
* **One site in a fully periodic unit domain (2D/3D):** every periodic image has the same radius; the single cell must occupy the complete fundamental domain (area or volume `1`).
* **Control and scale tests:** compare with small radii and larger domain lengths.

These examples distinguish a large **common radius offset** (which does not change the exact power diagram) from truly large **differences of squared radii**, whose geometric and numerical limitations must be studied separately.

## Run locally

Requirements: C++ compiler, Bash, `patch` (for the candidate only). No Python.

```bash
bash ./run.sh baseline
bash ./run.sh candidate
```

Use `CXX` and `CXXFLAGS` to select a compiler or flags:

```bash
CXX=clang++ CXXFLAGS='-O3 -std=c++11' bash ./run.sh baseline
```

The baseline run is **observational**: failures in the known high-radius cases are reported as evidence, rather than hiding them or treating the incorrect cell measure as the expected result. Candidate runs enforce correctness on the stated test matrix.

## Expected numerical invariants

* Total area/volume of all cells equals domain measure.
* Equal-radius sites have the same separator and cell extents as the corresponding unweighted sites.
* No gaps occur between the two analytic half-domain cells.
* A single site in a fully periodic domain owns all the domain measure.

These checks are implemented in the C++ programs, not in a Python wrapper. Negative neighbor IDs in periodic Voro++ containers are **not**, by themselves, evidence of a bug: they may refer to a site's own periodic image.

## Status and limitations

The source tree is pinned. The experimental patch is a **hypothesis under test**, not an accepted repair or a claim that every extreme weight range can be computed accurately. Inspect the CI and the checked results before treating a fix as demonstrated.

See [`docs/findings.md`](docs/findings.md) for arithmetic investigation and [`docs/issue-draft.md`](docs/issue-draft.md) for a proposed report to the Voro++ maintainer. No GitHub issue or pull request in Voro++ is created by this study.

## License

The original Voro++ source is redistributed under its license in [`vendor/voro/LICENSE`](vendor/voro/LICENSE). Reproducers and scripts are supplied for reproduction and technical analysis.
