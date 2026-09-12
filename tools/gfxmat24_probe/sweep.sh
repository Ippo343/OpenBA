#!/bin/sh
# tools/gfxmat24_probe/sweep.sh — Wave 24 lane GFXMAT
#
# The full control matrix, printed with EVERY CELL NAMED. Run from the repo
# root after building the probe:
#
#   g++ -std=c++17 -O0 -g -Wall -Wextra -Wno-cast-function-type \
#       -DGFXMAT24_PROBE -I. -Iruntime/types -Iruntime/funcs -Istandalone \
#       tools/gfxmat24_probe/probe.cpp runtime/src/graphics/wave24_lane_gfxmat.cpp \
#       -o tools/gfxmat24_probe/probe.exe
#   sh tools/gfxmat24_probe/sweep.sh
#
# Exit status of each cell is the verdict: 0 agreed, 1 diverged, 2 setup
# failure. A SETUP FAILURE IS NOT A PASS and is printed as its own word, because
# a cell that never ran reads as a blank line, which reads as neither.
P=tools/gfxmat24_probe/probe.exe

cell() {
    label="$1"; shift
    out=$("$P" "$@" 2>&1); rc=$?
    case $rc in
        0) v="agreed  " ;;
        1) v="DIVERGED" ;;
        *) v="SETUPFAIL" ;;
    esac
    printf '%-46s %s\n' "$label" "$v"
    if [ -n "$SWEEP_VERBOSE" ]; then printf '%s\n' "$out"; fi
}

echo "=== NULL CASES (must all agree) ==="
cell "write path, count=3 inject=3"        --mode write --count 3 --inject 3
cell "read  path, count=3 inject=3"        --mode read  --count 3 --inject 3
cell "write path, count=0 inject=0 (empty)" --mode write --count 0 --inject 0
cell "read  path, count=1 inject=1"        --mode read  --count 1 --inject 1

echo
echo "=== NEGATIVE CONTROLS, count==inject (control 6 must ABSTAIN) ==="
for n in 1 2 3 4 5 6; do
    cell "corrupt $n, read, count=3 inject=3" --mode read --count 3 --inject 3 --corrupt $n
done

echo
echo "=== THE REAL LOAD CASE, count != inject (control 6 must FIRE) ==="
cell "corrupt 0, read, count=2 inject=3"   --mode read --count 2 --inject 3
cell "corrupt 6, read, count=2 inject=3"   --mode read --count 2 --inject 3 --corrupt 6
