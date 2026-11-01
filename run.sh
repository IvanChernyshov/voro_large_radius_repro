#!/usr/bin/env bash
# Standalone Voro++ native reproduction. No Python, no source edits in vendor/voro.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODE="${1:-baseline}"
if [[ "$MODE" != baseline && "$MODE" != candidate ]]; then
  echo "Usage: bash ./run.sh [baseline|candidate]" >&2
  exit 2
fi
ORIGINAL="$ROOT/vendor/voro"
VORO="$ORIGINAL"
TMP=""
cleanup() { if [[ -n "$TMP" ]]; then rm -rf "$TMP"; fi; }
trap cleanup EXIT
if [[ "$MODE" == candidate ]]; then
  TMP="$(mktemp -d)"
  cp -R "$ORIGINAL" "$TMP/voro"
  (cd "$TMP/voro" && patch --batch --forward -p1 < "$ROOT/patches/candidate-01.patch")
  VORO="$TMP/voro"
fi
CXX="${CXX:-c++}"
CXXFLAGS="${CXXFLAGS:--O2 -std=c++11}"
BUILD="$ROOT/build/$MODE"
OUTPUT="$ROOT/output/$MODE"
mkdir -p "$BUILD" "$OUTPUT"
echo "mode=$MODE compiler=$CXX flags=$CXXFLAGS"
"$CXX" $CXXFLAGS -I"$VORO/2d/src" "$ROOT/repro/repro2d.cpp" "$VORO/2d/src/voro++_2d.cc" -o "$BUILD/repro2d"
"$CXX" $CXXFLAGS -I"$VORO/src" "$ROOT/repro/repro3d.cpp" "$VORO/src/voro++.cc" -o "$BUILD/repro3d"
violations=0
known_failures=0
for dim in 2 3; do
  for config in "small 1 1 1" "moderate 1000000 1 1" "threshold 10000000 1 1" "extreme 100000000 1 1" "multiblock 100000000 1 2" "rescaled 100000000 100 1"; do
    read -r label r L blocks <<< "$config"
    outfile="$OUTPUT/${dim}d_${label}.txt"
    if "$BUILD/repro${dim}d" "$r" "$L" "$blocks" > "$outfile" 2>&1; then
      outcome="PASS"
    else
      outcome="FAIL"
    fi
    echo "${MODE} ${dim}D ${label} : ${outcome}"
    if [[ "$MODE" == candidate || "$label" == small || "$label" == moderate || "$label" == threshold || "$label" == rescaled ]]; then
      if [[ "$outcome" == FAIL ]]; then violations=$((violations+1)); fi
    elif [[ "$outcome" == FAIL ]]; then
      known_failures=$((known_failures+1))
    fi
  done
done
if [[ "$MODE" == baseline ]]; then
  echo "Original Voro++: $known_failures failing extreme configurations observed (informational)."
fi
echo "Required checks failed: $violations"
if [[ "$violations" != 0 ]]; then exit 1; fi

# Focused arithmetic regression probes (A1-A3). In this research phase their
# failures are observed evidence, not a reason to mark the workflow successful
# results as geometrically correct. A program error / invalid oracle remains fatal.
for dim in 2 3; do
  if [[ "$dim" == 2 ]]; then
    VORO_INC="$VORO/2d/src"
    VORO_UNIT="$VORO/2d/src/voro++_2d.cc"
  else
    VORO_INC="$VORO/src"
    VORO_UNIT="$VORO/src/voro++.cc"
  fi
  "$CXX" $CXXFLAGS -I"$VORO_INC" \
    "$ROOT/repro/arithmetic${dim}d.cpp" "$VORO_UNIT" -o "$BUILD/arithmetic${dim}d"
  logfile="$OUTPUT/${dim}d_arithmetic.txt"
  rc=0
  "$BUILD/arithmetic${dim}d" > "$logfile" 2>&1 || rc=$?
  cat "$logfile"
  if [[ "$rc" -gt 1 ]]; then
    echo "Arithmetic probe ${dim}D terminated unexpectedly with status $rc" >&2
    exit "$rc"
  fi
  if [[ "$rc" -eq 1 ]]; then
    echo "Arithmetic probe ${dim}D: numerical differences recorded (informational)."
  fi
done
