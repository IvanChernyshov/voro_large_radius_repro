#!/usr/bin/env bash
# Research-only dev-branch baseline repro. Does not touch master or any patch.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VORO="$ROOT/vendor/voro-dev"
BUILD="$ROOT/build/dev-baseline"
OUTPUT="$ROOT/output/dev-baseline"
CXX="${CXX:-c++}"
CXXFLAGS="${CXXFLAGS:--O2 -std=c++11}"
mkdir -p "$BUILD" "$OUTPUT"
echo "dev master-independent snapshot=61099f682c6672a275b6be7de5923f2a1a7bf05e"
echo "mode=dev-baseline compiler=$CXX flags=$CXXFLAGS"
# The dev branch builds 2D and 3D together; unlike master, they share voro++.cc.
"$CXX" $CXXFLAGS -pthread -I"$VORO/src" -c "$VORO/src/voro++.cc" -o "$BUILD/voro-dev.o"
for dim in 2 3; do
  "$CXX" $CXXFLAGS -pthread -I"$VORO/src" \
    "$ROOT/repro/dev/minimal${dim}d.cpp" "$BUILD/voro-dev.o" -o "$BUILD/minimal${dim}d"
  logfile="$OUTPUT/${dim}d_minimal.txt"
  rc=0
  "$BUILD/minimal${dim}d" > "$logfile" 2>&1 || rc=$?
  cat "$logfile"
  # Baseline must reproduce *both* native faults: not an arbitrary nonzero exit.
  if [[ "$rc" != 1 ]]; then
    echo "dev ${dim}D: expected both native checks to report failures, exit=$rc." >&2
    exit 1
  fi
  for check in geometry owner; do
    if ! grep -Eq "^RESULT dim=${dim} check=${check} status=FAIL[[:space:]]" "$logfile"; then
      echo "dev ${dim}D: did not reproduce $check failure." >&2
      exit 1
    fi
  done
  if ! grep -qx "SUMMARY minimal dim=${dim} failed_checks=2" "$logfile"; then
    echo "dev ${dim}D: incomplete/erroring reproduction." >&2
    exit 1
  fi
done
echo "SUMMARY dev-baseline status=PASS reproduced_geometry_and_owner_2d_3d=4"
