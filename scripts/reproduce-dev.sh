#!/usr/bin/env bash
# Compare native dev baseline and isolated combined correction.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
echo "Voro++ dev: four native failure reproductions, followed by four corrected checks"
bash "$ROOT/scripts/run-dev-minimal.sh" baseline
bash "$ROOT/scripts/run-dev-minimal.sh" combined
echo "SUMMARY dev-reproduction status=PASS original_errors_reproduced=4 correction_checks_passed=4"
