#!/usr/bin/env bash
# tools/uif6_run.sh — Wave 10C lane UIF6's verification pass.
#
# Deploys THIS worktree's debug exe and drives the two routes that reach the six
# ported bodies, in one lock hold so nothing can land between the copy and the
# launches:
#
#   A  gameplay + the utility dialog  -> IsInsideClipRegion, EndDragDrop,
#      EndHover, BeginToolTip (all called from element_vtable.cpp, live under
#      BANISHED_RT_ELEMVTABLE). Lane ELEM's route.
#   B  the modal self-test            -> Update/DrawStandAloneUI, which only run
#      inside ShowDialog's nested MainLoop pump. Lane SHOW's route.
#
# Everything is archived RAW: a truncated log manufactures the calls=0 signal
# this project reads ports on
# (reference_truncated_tool_output_manufactures_calls_zero.md).
set -u

REPO="$(cd "$(dirname "$0")/.." && pwd)"
GAME_DIR="${BANISHED_DIR:-C:/GOG Games/Banished}"
OUT="$REPO/lane_uif6_logs"
mkdir -p "$OUT"

echo "=== deploying $(stat -c %y "$REPO/standalone/build/reimpl-x64.exe")"
cp -f "$REPO/standalone/build/reimpl-x64.exe" "$GAME_DIR/reimpl-x64.exe" || exit 2

run_cell() {
    local name="$1"; shift
    echo
    echo "############ CELL $name ############"
    "$REPO/tools/rt_run.sh" "$name" --timeout 420 -- "$@" >"$OUT/$name.rt_run.txt" 2>&1
    local rc=$?
    cp -f "$GAME_DIR/standalone_error.log" "$OUT/$name.err.log" 2>/dev/null
    cp -f "$GAME_DIR/standalone_debug.log" "$OUT/$name.out.log" 2>/dev/null
    [ -s "$GAME_DIR/standalone_crash.log" ] && cp -f "$GAME_DIR/standalone_crash.log" "$OUT/$name.crash.log"
    echo "rc=$rc"
    echo "--- the six entry counters (RAW, untruncated) ---"
    grep -a -E 'IsInsideClipRegion|EndDragDrop|EndHover|BeginToolTip|StandAloneUI' \
        "$OUT/$name.err.log" "$OUT/$name.out.log" 2>/dev/null | grep -a -E '\[RtPort\]|\[UIF6'
    echo "--- UI registration count ---"
    grep -a 'UI: .* exports registered' "$OUT/$name.err.log" "$OUT/$name.out.log" 2>/dev/null
    echo "--- crash ---"
    if [ -s "$OUT/$name.crash.log" ]; then head -20 "$OUT/$name.crash.log"; else echo "(none)"; fi
    echo "--- shutdown ---"
    grep -a 'Shutdown\] done' "$OUT/$name.err.log" "$OUT/$name.out.log" 2>/dev/null || echo "(no clean shutdown line)"
}

# --- A: gameplay, with the utility dialog opened headlessly -----------------
run_cell A_utility \
    BANISHED_AUTOSTART=1 \
    BANISHED_TOOL_HARNESS=utility \
    'BANISHED_UTILITY_TEMPLATE=Template\UtilityMap.rsc' \
    BANISHED_UIF6_SWEEP=1 \
    BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120

# --- B: the nested modal pump ------------------------------------------------
run_cell B_modal \
    BANISHED_AUTOQUIT=1 \
    BANISHED_MODAL_DRIVER=2 \
    BANISHED_MODAL_DRIVER_TEST=1

echo
echo "=== logs archived under $OUT"
