#!/usr/bin/env bash
# Dev branch native minimal reproduction, with an independent, isolated patch.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODE="${1:-baseline}"
if [[ "$MODE" != baseline && "$MODE" != combined ]]; then
  echo "Usage: bash scripts/run-dev-minimal.sh [baseline|combined]" >&2
  exit 2
fi
ORIGINAL="$ROOT/vendor/voro-dev"
VORO="$ORIGINAL"
TMP=""
cleanup() { if [[ -n "$TMP" ]]; then rm -rf "$TMP"; fi; }
trap cleanup EXIT
if [[ "$MODE" == combined ]]; then
  TMP="$(mktemp -d)"
  cp -R "$ORIGINAL" "$TMP/voro"
  (cd "$TMP/voro" && patch --batch --forward -p1 < "$ROOT/patches/dev/candidate-02-plus-03.patch")
  VORO="$TMP/voro"
fi
BUILD="$ROOT/build/dev-$MODE"
OUTPUT="$ROOT/output/dev-$MODE"
CXX="${CXX:-c++}"
CXXFLAGS="${CXXFLAGS:--O2 -std=c++11}"
mkdir -p "$BUILD" "$OUTPUT"
echo "dev snapshot=61099f682c6672a275b6be7de5923f2a1a7bf05e mode=$MODE compiler=$CXX flags=$CXXFLAGS"
# The dev branch compiles the combined 2D/3D source from src/voro++.cc.
"$CXX" $CXXFLAGS -pthread -I"$VORO/src" -c "$VORO/src/voro++.cc" -o "$BUILD/voro-dev.o"
expected=FAIL; expected_rc=1; expected_failures=2
if [[ "$MODE" == combined ]]; then
  expected=PASS; expected_rc=0; expected_failures=0
fi
for dim in 2 3; do
  "$CXX" $CXXFLAGS -pthread -I"$VORO/src" \
    "$ROOT/repro/dev/minimal${dim}d.cpp" "$BUILD/voro-dev.o" -o "$BUILD/minimal${dim}d"
  logfile="$OUTPUT/${dim}d_minimal.txt"
  rc=0
  "$BUILD/minimal${dim}d" > "$logfile" 2>&1 || rc=$?
  cat "$logfile"
  if [[ "$rc" -ne "$expected_rc" ]]; then
    echo "dev $MODE ${dim}D: unexpected process status $rc (expected $expected_rc)." >&2
    exit 1
  fi
  for check in geometry owner; do
    if ! grep -Eq "^RESULT dim=${dim} check=${check} status=${expected}[[:space:]]" "$logfile"; then
      echo "dev $MODE ${dim}D: $check did not report the required $expected." >&2
      exit 1
    fi
  done
  if ! grep -qx "SUMMARY minimal dim=${dim} failed_checks=${expected_failures}" "$logfile"; then
    echo "dev $MODE ${dim}D: summary/check-count mismatch." >&2
    exit 1
  fi
done
echo "SUMMARY dev-minimal mode=$MODE status=PASS expected_native_result=$expected"
