#!/usr/bin/env bash
# Scope-only diagnostic. Original dev may fail in dedicated periodic image creation.
# Known native errors and geometry mismatches are recorded, not acceptance-gated.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CXX="${CXX:-c++}"
CXXFLAGS="${CXXFLAGS:--O2 -std=c++11}"
OUT="$ROOT/output/dev-triclinic"
mkdir -p "$OUT"
for mode in baseline combined; do
  TMP="$(mktemp -d)"
  trap 'rm -rf "$TMP"' EXIT
  cp -R "$ROOT/vendor/voro-dev" "$TMP/voro"
  if [[ "$mode" == combined ]]; then
    (cd "$TMP/voro" && patch --batch --forward -p1 < "$ROOT/patches/dev/candidate-02-plus-03.patch")
  fi
  BUILD="$ROOT/build/dev-triclinic-$mode"
  mkdir -p "$BUILD" "$OUT/$mode"
  "$CXX" $CXXFLAGS -pthread -I"$TMP/voro/src" -c "$TMP/voro/src/voro++.cc" -o "$BUILD/voro-dev.o"
  "$CXX" $CXXFLAGS -pthread -I"$TMP/voro/src" "$ROOT/repro/dev/triclinic_probe.cpp" "$BUILD/voro-dev.o" -o "$BUILD/triclinic_probe"
  passed=0; mismatch=0; known_internal=0; unexpected=0
  for scenario in single-small pair-small pair-large pair-adjacent nine-small; do
    for lattice in orth skew; do
      for blocks in 1 3; do
        for reverse in 0 1; do
          log="$OUT/$mode/${scenario}_${lattice}_b${blocks}_rev${reverse}.txt"
          rc=0
          "$BUILD/triclinic_probe" "$scenario" "$lattice" "$blocks" "$reverse" > "$log" 2>&1 || rc=$?
          kind=UNEXPECTED
          if [[ "$rc" == 0 ]] && grep -q 'status=PASS$' "$log"; then
            kind=PASS;passed=$((passed+1))
          elif [[ "$rc" == 1 ]] && grep -q 'status=GEOMETRY_MISMATCH$' "$log"; then
            kind=GEOMETRY_MISMATCH;mismatch=$((mismatch+1))
          elif [[ "$rc" == 3 ]] && grep -q 'Constructing periodic image for nonexistent point' "$log"; then
            kind=UPSTREAM_INTERNAL_ERROR;known_internal=$((known_internal+1))
          else
            unexpected=$((unexpected+1))
          fi
          printf 'PROBE mode=%s scenario=%s lattice=%s blocks=%s reverse=%s exit=%s result=%s\n' "$mode" "$scenario" "$lattice" "$blocks" "$reverse" "$rc" "$kind"
          if [[ "$kind" == UNEXPECTED ]]; then tail -8 "$log" >&2; fi
        done
      done
    done
  done
  # Broad A9 nine-site dedicated-triclinic sweep: diagnostic until reviewed.
  "$CXX" $CXXFLAGS -pthread -I"$TMP/voro/src" "$ROOT/repro/dev/stress3d.cpp" "$BUILD/voro-dev.o" -o "$BUILD/stress3d"
  stress_log="$OUT/$mode/A9_full_dedicated.txt"
  stress_rc=0
  "$BUILD/stress3d" > "$stress_log" 2>&1 || stress_rc=$?
  printf 'A9_DEDICATED mode=%s exit=%s\n' "$mode" "$stress_rc"
  grep '^SUMMARY A9 dim=3' "$stress_log" || tail -10 "$stress_log"
  printf 'SUMMARY DEV_TRICLINIC_DIAGNOSTIC mode=%s passed=%s mismatches=%s upstream_internal_errors=%s unexpected=%s\n' "$mode" "$passed" "$mismatch" "$known_internal" "$unexpected"
  rm -rf "$TMP"
  trap - EXIT
  if [[ "$unexpected" != 0 ]]; then exit 1; fi
done
