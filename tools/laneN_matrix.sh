#!/usr/bin/env bash
# tools/laneN_matrix.sh — Wave 9 lane N: re-run lane M's matrix with lane M's
# RELEASED-POINTER DETECTOR live, for the tree-wide list of sites that
# prefix-replay a body which swaps a refcounted pointer.
#
# This is NOT a re-measurement of lane M's divergence numbers (that row is
# lane M's). It answers one question the earlier runs could not: which OTHER
# sites hand the DLL a pointer our body already released. Every cell's raw log
# is archived by rt_matrix_cell.sh; this script just drives the list and then
# collates every "HARNESS DEFECT" line across all of them.
set -u
REPO="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$REPO/docs/runtime_port/wave9_lane_n"
mkdir -p "$OUT"

run() {  # run <cell> <timeout> -- ENV...
    local cell="$1"; shift
    local to="$1"; shift
    [ "${1:-}" = "--" ] && shift
    echo "########## $cell ##########"
    "$REPO/tools/rt_matrix_cell.sh" "laneN_$cell" "$to" -- "$@" 2>&1 | head -8
    cp -f "C:/GOG Games/Banished/standalone_error.log" "$OUT/$cell.err.log" 2>/dev/null
}

run menu_only   180 -- BANISHED_AUTOSTART=1 BANISHED_AUTOQUIT=1 BANISHED_RT_DIFF=1
run gameplay    300 -- BANISHED_AUTOSTART=1 BANISHED_RT_DIFF=1 BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=120 BANISHED_PAUSEMENU_BUTTON=6
run saveload    400 -- BANISHED_AUTOLOAD=Save9 BANISHED_RT_DIFF=1 BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=60 BANISHED_PAUSEMENU_BUTTON=6
run tutorial0   300 -- BANISHED_AUTOTUTORIAL=1 BANISHED_RT_DIFF=1 BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=60 BANISHED_PAUSEMENU_BUTTON=6
run tutorial02  300 -- BANISHED_AUTOTUTORIAL=02 BANISHED_RT_DIFF=1 BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=60 BANISHED_PAUSEMENU_BUTTON=6
run tutorial03  300 -- BANISHED_AUTOTUTORIAL=03 BANISHED_RT_DIFF=1 BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=60 BANISHED_PAUSEMENU_BUTTON=6
run menus_ach   180 -- BANISHED_AUTOMENU_BUTTON=8 BANISHED_AUTOMENU_CLICKS=m6 BANISHED_RT_DIFF=1
run menus_mods  180 -- BANISHED_AUTOMENU_BUTTON=7 BANISHED_AUTOMENU_CLICKS=101,m6 BANISHED_RT_DIFF=1
run stress      600 -- BANISHED_STRESS=1 BANISHED_STRESS_FRAMES=1200 BANISHED_RT_DIFF=1

echo
echo "================ COLLATED: released-pointer detector, ALL CELLS ================"
for f in "$OUT"/*.err.log; do
    st=$(grep -ac 'detector self-test: ALIVE' "$f" 2>/dev/null)
    echo "--- $(basename "$f") self-test-ALIVE=$st"
    grep -a 'HARNESS DEFECT (not a port defect)' "$f" 2>/dev/null \
        | sed 's/.*not a port defect) \*\*\* //; s/ call #.*//' | sort -u | sed 's/^/      /'
done
echo "================ DISTINCT SITES ACROSS THE WHOLE MATRIX ================"
grep -ah 'HARNESS DEFECT (not a port defect)' "$OUT"/*.err.log 2>/dev/null \
    | sed 's/.*not a port defect) \*\*\* //; s/ call #.*//' | sort -u
