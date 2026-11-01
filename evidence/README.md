# Reproduction evidence

Native output files are generated under `output/baseline/` and `output/candidate/`; they are deliberately ignored by Git. GitHub Actions uploads each profile's output as a workflow artifact together with the run's recorded environment and compiler version.

Source findings and the precise expectations are maintained in [the investigation](../docs/findings.md). The candidate patch is **not** merged into the original Voro++ source tree.

Do not treat a successful Actions job in the baseline profile as meaning all geometries were valid: that profile intentionally keeps known extreme failures informational while requiring control cases to pass.
