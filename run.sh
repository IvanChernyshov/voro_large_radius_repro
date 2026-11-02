#!/usr/bin/env bash
# Pinned Voro++ master reproduction and regression runner.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODE="${1:-baseline}"
SUITE="${2:-full}"
if [[ "$MODE" != baseline && "$MODE" != candidate && "$MODE" != candidate-02 && "$MODE" != candidate-03 && "$MODE" != combined-02-03 ]]; then
  echo "Usage: bash ./run.sh [baseline|candidate|candidate-02|candidate-03|combined-02-03] [full|minimal]" >&2
  exit 2
fi
if [[ "$SUITE" != full && "$SUITE" != minimal ]]; then
  echo "Unknown suite: $SUITE (expected full or minimal)" >&2
  exit 2
fi
if [[ "$SUITE" == minimal && "$MODE" != baseline && "$MODE" != combined-02-03 ]]; then
  echo "Minimal reproduction supports baseline and combined-02-03 only." >&2
  exit 2
fi
ORIGINAL="$ROOT/vendor/voro"
VORO="$ORIGINAL"
TMP=""
cleanup() { if [[ -n "$TMP" ]]; then rm -rf "$TMP"; fi; }
trap cleanup EXIT
if [[ "$MODE" != baseline ]]; then
  TMP="$(mktemp -d)"
  cp -R "$ORIGINAL" "$TMP/voro"
  PATCH_FILE="$ROOT/patches/candidate-01.patch"
  if [[ "$MODE" == candidate-02 ]]; then PATCH_FILE="$ROOT/patches/candidate-02.patch"; fi
  if [[ "$MODE" == candidate-03 ]]; then PATCH_FILE="$ROOT/patches/candidate-03.patch"; fi
  if [[ "$MODE" == combined-02-03 ]]; then PATCH_FILE="$ROOT/patches/candidate-02-plus-03.patch"; fi
  (cd "$TMP/voro" && patch --batch --forward -p1 < "$PATCH_FILE")
  VORO="$TMP/voro"
fi
CXX="${CXX:-c++}"
CXXFLAGS="${CXXFLAGS:--O2 -std=c++11}"
BUILD="$ROOT/build/$MODE"
OUTPUT="$ROOT/output/$MODE"
mkdir -p "$BUILD" "$OUTPUT"
echo "mode=$MODE compiler=$CXX flags=$CXXFLAGS"
# Compile each translation unit once and link the regression programs.
"$CXX" $CXXFLAGS -I"$VORO/2d/src" -c "$VORO/2d/src/voro++_2d.cc" -o "$BUILD/voro2d.o"
"$CXX" $CXXFLAGS -I"$VORO/src" -c "$VORO/src/voro++.cc" -o "$BUILD/voro3d.o"
# Minimal tests assert the precise original failures and the combined correction.
# A crash or unexpected output fails the check.
if [[ "$SUITE" == minimal ]]; then
  expected=FAIL
  expected_rc=1
  expected_failures=2
  if [[ "$MODE" == combined-02-03 ]]; then
    expected=PASS
    expected_rc=0
    expected_failures=0
  fi
  for dim in 2 3; do
    if [[ "$dim" == 2 ]]; then VORO_INC="$VORO/2d/src"; else VORO_INC="$VORO/src"; fi
    "$CXX" $CXXFLAGS -I"$VORO_INC" \
      "$ROOT/repro/minimal${dim}d.cpp" "$BUILD/voro${dim}d.o" -o "$BUILD/minimal${dim}d"
    logfile="$OUTPUT/${dim}d_minimal.txt"
    rc=0
    "$BUILD/minimal${dim}d" > "$logfile" 2>&1 || rc=$?
    cat "$logfile"
    if [[ "$rc" -ne "$expected_rc" ]]; then
      echo "Minimal $MODE ${dim}D: unexpected process status $rc (expected $expected_rc)." >&2
      exit 1
    fi
    for check in geometry owner; do
      if ! grep -Eq "^RESULT dim=${dim} check=${check} status=${expected}[[:space:]]" "$logfile"; then
        echo "Minimal $MODE ${dim}D: $check did not produce the required $expected outcome." >&2
        exit 1
      fi
    done
    if ! grep -qx "SUMMARY minimal dim=${dim} failed_checks=${expected_failures}" "$logfile"; then
      echo "Minimal $MODE ${dim}D: summary/check count mismatch." >&2
      exit 1
    fi
  done
  echo "SUMMARY minimal mode=$MODE status=PASS expected_native_result=$expected"
  exit 0
fi

"$CXX" $CXXFLAGS -I"$VORO/2d/src" "$ROOT/repro/repro2d.cpp" "$BUILD/voro2d.o" -o "$BUILD/repro2d"
"$CXX" $CXXFLAGS -I"$VORO/src" "$ROOT/repro/repro3d.cpp" "$BUILD/voro3d.o" -o "$BUILD/repro3d"
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
    if [[ "$MODE" == candidate || "$MODE" == candidate-02 || "$MODE" == combined-02-03 || "$label" == small || "$label" == moderate || "$label" == threshold || "$label" == rescaled ]]; then
      if [[ "$outcome" == FAIL ]]; then violations=$((violations+1)); fi
    elif [[ "$outcome" == FAIL ]]; then
      known_failures=$((known_failures+1))
    fi
  done
done
if [[ "$MODE" == baseline || "$MODE" == candidate-03 ]]; then
  echo "$MODE: $known_failures known cell-construction failures recorded (informational)."
fi
echo "Required checks failed: $violations"
if [[ "$violations" != 0 ]]; then exit 1; fi

# A1-A3: plane arithmetic and analytical two-site references.
# Baseline discrepancies are recorded; invalid references are fatal.
for dim in 2 3; do
  if [[ "$dim" == 2 ]]; then
    VORO_INC="$VORO/2d/src"
  else
    VORO_INC="$VORO/src"
  fi
  "$CXX" $CXXFLAGS -I"$VORO_INC" \
    "$ROOT/repro/arithmetic${dim}d.cpp" "$BUILD/voro${dim}d.o" -o "$BUILD/arithmetic${dim}d"
  logfile="$OUTPUT/${dim}d_arithmetic.txt"
  rc=0
  "$BUILD/arithmetic${dim}d" > "$logfile" 2>&1 || rc=$?
  cat "$logfile"
  if [[ "$rc" -gt 1 ]]; then
    echo "Arithmetic probe ${dim}D terminated unexpectedly with status $rc" >&2
    exit "$rc"
  fi
  if [[ "$rc" -eq 1 ]]; then
    if [[ "$MODE" == candidate-02 || "$MODE" == combined-02-03 ]]; then
      echo "Arithmetic probe ${dim}D: construction candidate must pass A1-A3." >&2
      exit 1
    fi
    echo "Arithmetic probe ${dim}D: numerical differences recorded (informational)."
  fi
done
# A4-A5: analytical owner queries; ordinary-radius controls must pass.
for dim in 2 3; do
  if [[ "$dim" == 2 ]]; then
    VORO_INC="$VORO/2d/src"
  else
    VORO_INC="$VORO/src"
  fi
  "$CXX" $CXXFLAGS -I"$VORO_INC" \
    "$ROOT/repro/locate${dim}d.cpp" "$BUILD/voro${dim}d.o" -o "$BUILD/locate${dim}d"
  logfile="$OUTPUT/${dim}d_location.txt"
  rc=0
  "$BUILD/locate${dim}d" > "$logfile" 2>&1 || rc=$?
  grep -E '^(PARAM|CASE |SCENARIO |SUMMARY )' "$logfile"
  if [[ "$rc" -gt 1 ]]; then
    cat "$logfile" >&2
    echo "Locate ${dim}D control or internal-oracle failure; status=$rc" >&2
    exit "$rc"
  fi
  if [[ "$rc" -eq 1 ]]; then
    if [[ "$MODE" == candidate-03 || "$MODE" == combined-02-03 ]]; then
      echo "Location candidate must pass A4-A5 location tests." >&2
      exit 1
    fi
    echo "Locate ${dim}D: numerical failures recorded in $logfile (informational)."
  fi
done
# A6-A7: rectangular and dedicated periodic cells, with ordinary-radius controls.
for dim in 2 3; do
  if [[ "$dim" == 2 ]]; then
    VORO_INC="$VORO/2d/src"
  else
    VORO_INC="$VORO/src"
  fi
  "$CXX" $CXXFLAGS -I"$VORO_INC" \
    "$ROOT/repro/coverage${dim}d.cpp" "$BUILD/voro${dim}d.o" -o "$BUILD/coverage${dim}d"
  logfile="$OUTPUT/${dim}d_coverage.txt"
  rc=0
  "$BUILD/coverage${dim}d" > "$logfile" 2>&1 || rc=$?
  grep '^SUMMARY A6_A7' "$logfile"
  if [[ "$rc" -gt 1 ]] || [[ ( "$MODE" == candidate || "$MODE" == candidate-02 || "$MODE" == combined-02-03 ) && "$rc" -ne 0 ]]; then
    grep 'status=FAIL' "$logfile" | head -50 >&2 || true
    echo "A6-A7 ${dim}D control/candidate failure: exit=$rc" >&2
    exit "$rc"
  fi
  if [[ "$rc" -eq 1 ]]; then
    echo "A6-A7 ${dim}D: original large-radius failures recorded (informational)."
  fi
done

# Stage 6 (A8): unequal-radius multi-site geometry in rectangular and
# dedicated periodic containers. References: analytic 1D inequalities
# (rectangular), and exhaustive unpruned periodic image clipping (3D).
# Original Voro++ and candidate-01 discrepancies are diagnostic.
# Candidate-02 is required to match these selected independent references.
for dim in 2 3; do
  if [[ "$dim" == 2 ]]; then
    VORO_INC="$VORO/2d/src"
  else
    VORO_INC="$VORO/src"
  fi
  "$CXX" $CXXFLAGS -I"$VORO_INC" \
    "$ROOT/repro/unequal${dim}d.cpp" "$BUILD/voro${dim}d.o" -o "$BUILD/unequal${dim}d"
  logfile="$OUTPUT/${dim}d_unequal.txt"
  rc=0
  "$BUILD/unequal${dim}d" > "$logfile" 2>&1 || rc=$?
  grep '^SUMMARY A8' "$logfile"
  if [[ "$rc" -gt 1 ]] || [[ ( "$MODE" == candidate-02 || "$MODE" == combined-02-03 ) && "$rc" -ne 0 ]]; then
    grep 'status=FAIL' "$logfile" | head -40 >&2 || true
    grep -E "^(ORACLE_|SUMMARY A8)" "$logfile" >&2 || true
    echo "A8 ${dim}D control/oracle/candidate-02 failure (exit=$rc)" >&2
    exit 1
  fi
  if [[ "$rc" -eq 1 ]]; then
    echo "A8 ${dim}D: discrepancies recorded for ${MODE} (informational)."
  fi
done

# Stage 7 (A9): non-collinear/noncoplanar deterministic stress tests.
# 2D: long-double polygon reference independent of the Voro++ clipper.
# 3D: exhaustive direct-plane clipping without radius_poly or block pruning.
# Original/candidate-01 mismatches are diagnostic; candidate-02 must pass.
for dim in 2 3; do
  if [[ "$dim" == 2 ]]; then VORO_INC="$VORO/2d/src"; else VORO_INC="$VORO/src"; fi
  "$CXX" $CXXFLAGS -I"$VORO_INC" \
    "$ROOT/repro/stress${dim}d.cpp" "$BUILD/voro${dim}d.o" -o "$BUILD/stress${dim}d"
  logfile="$OUTPUT/${dim}d_stress.txt"
  rc=0
  "$BUILD/stress${dim}d" > "$logfile" 2>&1 || rc=$?
  grep "^SUMMARY A9" "$logfile"
  if [[ "$rc" -gt 1 ]] || [[ ( "$MODE" == candidate-02 || "$MODE" == combined-02-03 ) && "$rc" -ne 0 ]]; then
    grep -E "^(FAIL_CASE|ORACLE_|SUMMARY A9)" "$logfile" | head -55 >&2 || true
    echo "A9 ${dim}D oracle/control/candidate-02 failure (exit=$rc)" >&2
    exit 1
  fi
  if [[ "$rc" -eq 1 ]]; then
    echo "A9 ${dim}D: ${MODE} numerical discrepancies recorded (informational)."
  fi
done

# Stage 8 (A10): exhaustive, shifted-power nearest-site oracle in
# 2D/3D, including periodic-image coordinates, masks, skew lattice,
# insertion orders and spatial block grids. Only candidate-03 must pass.
for dim in 2 3; do
  if [[ "$dim" == 2 ]]; then VORO_INC="$VORO/2d/src"; else VORO_INC="$VORO/src"; fi
  "$CXX" $CXXFLAGS -I"$VORO_INC" \
    "$ROOT/repro/owner${dim}d.cpp" "$BUILD/voro${dim}d.o" -o "$BUILD/owner${dim}d"
  logfile="$OUTPUT/${dim}d_owner.txt"
  rc=0
  "$BUILD/owner${dim}d" > "$logfile" 2>&1 || rc=$?
  grep "^SUMMARY A10" "$logfile"
  if [[ "$rc" -gt 1 ]] || [[ ( "$MODE" == candidate-03 || "$MODE" == combined-02-03 ) && "$rc" -ne 0 ]]; then
    grep -E "^(WRONG|ORACLE_|SUMMARY A10)" "$logfile" | head -50 >&2 || true
    echo "A10 ${dim}D oracle/control/candidate-03 failure (exit=$rc)" >&2
    exit 1
  fi
  if [[ "$rc" -eq 1 ]]; then
    echo "A10 ${dim}D: known owner-query errors in ${MODE} (informational)."
  fi
done

# Stage 9 A11: extreme finite radii, external periodic queries, and near ties.
# Only the search-specific candidate-03 must pass supported nonnegative inputs.
# Negative radii and score margins <=1e-12 remain diagnostics.
for dim in 2 3; do
  if [[ "$dim" == 2 ]]; then VORO_INC="$VORO/2d/src"; else VORO_INC="$VORO/src"; fi
  "$CXX" $CXXFLAGS -I"$VORO_INC" \
    "$ROOT/repro/extreme${dim}d.cpp" "$BUILD/voro${dim}d.o" -o "$BUILD/extreme${dim}d"
  logfile="$OUTPUT/${dim}d_extreme.txt"
  rc=0
  "$BUILD/extreme${dim}d" > "$logfile" 2>&1 || rc=$?
  grep "^SUMMARY A11" "$logfile"
  if [[ "$rc" -gt 1 ]] || [[ ( "$MODE" == candidate-03 || "$MODE" == combined-02-03 ) && "$rc" -ne 0 ]]; then
    grep -E "^(ORACLE_|MISMATCH|SUMMARY A11)" "$logfile" | head -40 >&2 || true
    echo "A11 ${dim}D fatal oracle or candidate-03 search failure (exit=$rc)" >&2
    exit 1
  fi
  if [[ "$rc" -eq 1 ]]; then echo "A11 ${dim}D: known owner-query failures in ${MODE}."; fi
done

# Stage 9 A12: equal-radius cell measure at extreme exponents.
# 1e308 is diagnostic even for candidate-02; supported exponent checks
# are enforced for candidate-02, not for the untouched search-only patch.
for dim in 2 3; do
  if [[ "$dim" == 2 ]]; then VORO_INC="$VORO/2d/src"; else VORO_INC="$VORO/src"; fi
  "$CXX" $CXXFLAGS -I"$VORO_INC" \
    "$ROOT/repro/exponent${dim}d.cpp" "$BUILD/voro${dim}d.o" -o "$BUILD/exponent${dim}d"
  logfile="$OUTPUT/${dim}d_exponent.txt"
  rc=0
  "$BUILD/exponent${dim}d" > "$logfile" 2>&1 || rc=$?
  grep "^SUMMARY A12" "$logfile"
  if [[ "$rc" -gt 1 ]] || [[ ( "$MODE" == candidate-02 || "$MODE" == combined-02-03 ) && "$rc" -ne 0 ]]; then
    grep -E "^(CASE A12|SUMMARY A12)" "$logfile" | tail -20 >&2 || true
    echo "A12 ${dim}D supported-range geometry failure (exit=$rc)" >&2
    exit 1
  fi
done

# Stage 11: scalar r_ctest/r_prime/r_cutoff diagnostics.
# Their results are logged separately from geometric acceptance gates.
for dim in 2 3; do
  if [[ "$dim" == 2 ]]; then
    VORO_INC="$VORO/2d/src"
    FLAGS=(-DAUDIT_2D)
  else
    VORO_INC="$VORO/src"
    FLAGS=()
  fi
  logfile="$OUTPUT/${dim}d_pruning_scope.txt"
  if "$CXX" $CXXFLAGS "${FLAGS[@]}" -I"$VORO_INC" \
    "$ROOT/repro/radius_bounds.cpp" -o "$BUILD/radius_bounds${dim}d" > "$OUTPUT/${dim}d_pruning_build.txt" 2>&1; then
    rc=0
    "$BUILD/radius_bounds${dim}d" > "$logfile" 2>&1 || rc=$?
    if [[ "$rc" == 0 ]]; then
      grep "^SUMMARY radius_poly" "$logfile" || true
    else
      echo "Scope-only ${dim}D probe exited with $rc (not an acceptance gate)." >&2
    fi
  else
    echo "Scope-only ${dim}D probe compilation failed (not an acceptance gate)." >&2
    tail -15 "$OUTPUT/${dim}d_pruning_build.txt" >&2 || true
  fi
done
