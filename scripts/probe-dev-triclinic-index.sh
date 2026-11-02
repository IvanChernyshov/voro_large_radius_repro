#!/usr/bin/env bash
# Trace out-of-range indices caused by the old test loop using != end().
# Instrument a temporary copy of the pinned dev source.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
cp -R "$ROOT/vendor/voro-dev" "$TMP/voro"
HEADER="$TMP/voro/src/container_tri.hh"
python3 - "$HEADER" <<'PY'
from pathlib import Path
import sys
path=Path(sys.argv[1])
text=path.read_text()
needle="            create_periodic_image(qi,qj,qk);"
trace="""            if(qi<0||qi>=nx||qj<0||qj>=oy||qk<0||qk>=oz)
                std::fprintf(stderr,"IMAGE_INDEX_TRACE ci=%d cj=%d ck=%d ei=%d ej=%d ek=%d qi=%d qj=%d qk=%d nx=%d ny=%d nz=%d ey=%d ez=%d oy=%d oz=%d\\n",
                    ci,cj,ck,ei,ej,ek,qi,qj,qk,nx,ny,nz,ey,ez,oy,oz);
            create_periodic_image(qi,qj,qk);"""
if text.count(needle)!=1:
    raise RuntimeError("Cannot uniquely locate dev region_index call")
text=text.replace(needle,trace)
fatal='''            if(di<0||di>=nx||dj<0||dj>=oy||dk<0||dk>=oz)
                voro_fatal_error("Constructing periodic image for nonexistent point",VOROPP_INTERNAL_ERROR);'''
replacement='''            if(di<0||di>=nx||dj<0||dj>=oy||dk<0||dk>=oz) {
                std::fprintf(stderr,"IMAGE_FATAL_TRACE di=%d dj=%d dk=%d nx=%d oy=%d oz=%d\\n",
                    di,dj,dk,nx,oy,oz);
                voro_fatal_error("Constructing periodic image for nonexistent point",VOROPP_INTERNAL_ERROR);
            }'''
if text.count(fatal)!=1:
    raise RuntimeError("Cannot uniquely locate dev fatal bounds check")
text=text.replace(fatal,replacement)
path.write_text(text)
PY
BUILD="$TMP/build"
mkdir -p "$BUILD" "$ROOT/output/dev-triclinic/index-trace"
CXX="${CXX:-g++}"
"$CXX" -O0 -g -std=c++11 -pthread -I"$TMP/voro/src" -c "$TMP/voro/src/voro++.cc" -o "$BUILD/voro.o"
"$CXX" -O0 -g -std=c++11 -pthread -I"$TMP/voro/src" "$ROOT/repro/dev/triclinic_probe.cpp" "$BUILD/voro.o" -o "$BUILD/probe"
for spec in "single-small orth 1 0" "pair-small orth 1 0" "pair-small skew 3 0" "nine-small orth 1 0"; do
    read -r scenario lattice blocks reverse <<<"$spec"
    logfile="$ROOT/output/dev-triclinic/index-trace/${scenario}_${lattice}_b${blocks}.txt"
    rc=0
    VORO_DEV_TRICLINIC_LEGACY_ITERATOR=1 "$BUILD/probe" "$scenario" "$lattice" "$blocks" "$reverse" >"$logfile" 2>&1 || rc=$?
    printf 'INDEX_PROBE scenario=%s lattice=%s blocks=%s exit=%s\n' "$scenario" "$lattice" "$blocks" "$rc"
    grep -E 'IMAGE_(INDEX|FATAL)_TRACE|Constructing periodic image|RESULT DEV_TRICLINIC' "$logfile" | head -12 || true
done
echo "SUMMARY DEV_TRICLINIC_INDEX_TRACE diagnostic_only=1"
