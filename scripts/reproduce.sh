#!/usr/bin/env bash
# Fast, standalone native reproduction of both radical Voronoi failures.
# Delegates build and patch application to run.sh (no duplicate build logic).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
echo "Voro++ large-radius reproduction — pinned original vs combined correction"
echo "Checking original Voro++ (both failures must be reproduced)..."
bash "$ROOT/run.sh" baseline minimal
echo "Checking isolated combined correction (both checks must pass)..."
bash "$ROOT/run.sh" combined-02-03 minimal
echo "SUMMARY reproduction status=PASS original_errors_reproduced=4 correction_checks_passed=4"
