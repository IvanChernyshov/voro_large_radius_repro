# Reproduction evidence

The native test runners write measurements to `output/<profile>/`
for the pinned `master` and to `output/dev-baseline/` and
`output/dev-combined/` for `dev`. Git ignores these generated files;
GitHub Actions preserves the test logs as workflow artifacts.

Baseline jobs compare native results with analytical or independently
computed references and record the known large-radius mismatches.
A successful baseline job indicates that its controls and expected
failure checks behaved as specified. The combined-patch jobs require
the supported regression cases to match their references.

The numerical results and test definitions are summarized in the
[README](../README.md), [combined `master` report](../docs/test-stage10-combined.md),
and [`dev` report](../docs/dev-branch.md).
