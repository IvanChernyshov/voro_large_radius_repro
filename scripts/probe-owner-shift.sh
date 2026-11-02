#!/usr/bin/env bash
# Diagnostic only: original owner search is correct; the shifted patch can regress.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
CXX="${CXX:-c++}"
CXXFLAGS="${CXXFLAGS:--O2 -std=c++11}"
OUTPUT="$ROOT/output/owner-shift"
mkdir -p "$OUTPUT"
patched_wrong=0
for branch in master dev; do
  if [[ "$branch" == master ]]; then original="$ROOT/vendor/voro"
  else original="$ROOT/vendor/voro-dev"; fi
  for mode in baseline combined; do
    src="$original"
    if [[ "$mode" == combined ]]; then
      src="$TMP/$branch-combined"
      cp -R "$original" "$src"
      if [[ "$branch" == master ]]; then
        patch_file="$ROOT/patches/candidate-02-plus-03.patch"
      else
        patch_file="$ROOT/patches/dev/candidate-02-plus-03.patch"
      fi
      (cd "$src" && patch --batch --forward -p1 < "$patch_file")
    fi
    if [[ "$branch" == master ]]; then
      "$CXX" $CXXFLAGS -I"$src/2d/src" -c "$src/2d/src/voro++_2d.cc" -o "$TMP/master-$mode-2d.o"
      "$CXX" $CXXFLAGS -I"$src/src" -c "$src/src/voro++.cc" -o "$TMP/master-$mode-3d.o"
    else
      "$CXX" $CXXFLAGS -pthread -I"$src/src" -c "$src/src/voro++.cc" -o "$TMP/dev-$mode.o"
    fi
    for dim in 2 3; do
      defs=()
      if [[ "$dim" == 2 ]]; then defs+=("-DOWNER_SHIFT_2D"); fi
      if [[ "$branch" == dev ]]; then
        defs+=("-DOWNER_SHIFT_DEV")
        inc="$src/src"
        obj="$TMP/dev-$mode.o"
      elif [[ "$dim" == 2 ]]; then
        inc="$src/2d/src"
        obj="$TMP/master-$mode-2d.o"
      else
        inc="$src/src"
        obj="$TMP/master-$mode-3d.o"
      fi
      exe="$TMP/owner-$branch-$mode-$dim"
      "$CXX" $CXXFLAGS -pthread "${defs[@]}" -I"$inc" "$ROOT/repro/owner_shift.cpp" "$obj" -o "$exe"
      for scenario in large scaled; do
        logfile="$OUTPUT/$branch-$mode-${dim}d-$scenario.txt"
        rc=0
        "$exe" "$scenario" > "$logfile" 2>&1 || rc=$?
        cat "$logfile"
        if [[ "$rc" -ne 0 ]] || ! grep -Eq "^RESULT owner_shift branch=$branch dim=$dim case=$scenario status=(CORRECT|WRONG) " "$logfile"; then
          echo "Owner-shift diagnostic failed to report a result: $logfile (exit $rc)" >&2
          exit 1
        fi
        if [[ "$mode" == baseline ]] && ! grep -q ' status=CORRECT ' "$logfile"; then
          echo "Original $branch unexpectedly gave the wrong owner ($dim D, $scenario)." >&2
          exit 1
        fi
        if [[ "$mode" == combined ]] && grep -q ' status=WRONG ' "$logfile"; then
          patched_wrong=$((patched_wrong+1))
        fi
      done
    done
  done
done
echo "SUMMARY owner_shift diagnostic_only=1 baseline_correct=1 patched_wrong=$patched_wrong/8"
