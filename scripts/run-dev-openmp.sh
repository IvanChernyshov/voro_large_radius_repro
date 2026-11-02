#!/usr/bin/env bash
# OpenMP concurrency regression for the isolated dev-branch numerical correction.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:--O2 -std=c++11}"
BUILD="$ROOT/build/dev-openmp"
OUTPUT="$ROOT/output/dev-openmp"
mkdir -p "$BUILD" "$OUTPUT"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
cp -R "$ROOT/vendor/voro-dev" "$TMP/voro"
(cd "$TMP/voro" && patch --batch --forward -p1 < "$ROOT/patches/dev/candidate-02-plus-03.patch")
"$CXX" $CXXFLAGS -fopenmp -pthread -I"$TMP/voro/src" \
  -c "$TMP/voro/src/voro++.cc" -o "$BUILD/voro-dev-omp.o"
"$CXX" $CXXFLAGS -fopenmp -pthread -I"$TMP/voro/src" \
  "$ROOT/repro/dev/thread_safety.cpp" "$BUILD/voro-dev-omp.o" -o "$BUILD/thread_safety"
OMP_DYNAMIC=false OMP_NUM_THREADS=4 "$BUILD/thread_safety" | tee "$OUTPUT/openmp.txt"
grep -qx "SUMMARY DEV_OMP total_failures=0" "$OUTPUT/openmp.txt"
