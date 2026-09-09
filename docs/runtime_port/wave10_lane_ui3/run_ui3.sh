#!/usr/bin/env bash
# =============================================================================
# docs/runtime_port/wave10_lane_ui3/run_ui3.sh — lane UI3's three verification
# runs, under ONE lock hold.
# =============================================================================
#
# Invoked as
#     tools/game_dir_lock.sh run lane-UI3 2400 -- \
#         docs/runtime_port/wave10_lane_ui3/run_ui3.sh
# so acquire/release is the wrapper's job and its exit status is checked inside
# `cmd_run` rather than by a pipeline that discards it.
#
# WHY ALL THREE RUNS SHARE ONE HOLD: three separate `run` invocations would give
# up the lock between them, and another lane's deploy landing in a gap would
# make run 2 and run 3 measure a different binary from run 1 -- the exact
# failure the integrator reported tonight. One hold, three launches, the md5
# re-checked immediately before each.
#
# THE MD5 CHECK IS THE POINT, not decoration. `verify` says we still hold the
# lock; the md5 says the bytes in the game directory are still OUR build. They
# are different questions and both have failed in this project.
set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
cd "$ROOT" || exit 2
GAME_DIR="${BANISHED_DIR:-C:/GOG Games/Banished}"
OURS="standalone/build/reimpl-x64.exe"
OUT="$HERE"

deploy_and_check() {
    tools/game_dir_lock.sh verify lane-UI3 || return 1
    cp "$OURS" "$GAME_DIR/reimpl-x64.exe" || return 1
    local a b
    a=$(md5sum "$OURS"                     | cut -d' ' -f1)
    b=$(md5sum "$GAME_DIR/reimpl-x64.exe"  | cut -d' ' -f1)
    echo "[ui3] build md5    = $a"
    echo "[ui3] deployed md5 = $b"
    if [ "$a" != "$b" ]; then
        echo "[ui3] *** MD5 MISMATCH -- the deployed exe is NOT our build. ABORTING. ***"
        return 1
    fi
    return 0
}

one_run() {   # one_run <label> <env...>
    local label="$1"; shift
    echo "================ [ui3] run: $label ================"
    deploy_and_check || return 1
    tools/rt_run.sh "$label" --timeout 600 -- "$@" > "$OUT/run_$label.txt" 2>&1
    local rc=$?
    echo "[ui3] rt_run rc=$rc  -> $OUT/run_$label.txt"
    cp "$GAME_DIR/standalone_debug.log" "$OUT/${label}_debug.log" 2>/dev/null
    cp "$GAME_DIR/standalone_error.log" "$OUT/${label}_error.log" 2>/dev/null
    cp "$GAME_DIR/standalone_crash.log" "$OUT/${label}_crash.log" 2>/dev/null
    echo "---- [RtPort] lines from this run ----"
    grep -a "RtPort\]   \|RtAlloc\|negative control" \
        "$OUT/${label}_debug.log" "$OUT/${label}_error.log" 2>/dev/null | head -60
    echo "---- StartTimer / AddController / DialogInstance diff sites ----"
    grep -a "StartTimer\|AddController\|DialogInstance" \
        "$OUT/${label}_debug.log" "$OUT/${label}_error.log" 2>/dev/null | head -40
    return $rc
}

# 1. DEFAULT configuration -- BANISHED_RT_ALLOC deliberately UNSET. This is the
#    one that matters: the lane's claim is that nothing here depends on the
#    armed allocator, and this run is where that claim is tested.
one_run default BANISHED_AUTOSTART=1 BANISHED_RT_DIFF=1 \
                BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=60
RC1=$?

# 2. ARMED -- the ported allocator serving the same four bodies.
one_run armed   BANISHED_AUTOSTART=1 BANISHED_RT_DIFF=1 BANISHED_RT_ALLOC=1 \
                BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=60
RC2=$?

# 3. KILL SWITCH -- all four rows back on the DLL. This is the control that says
#    a clean run 1 is attributable to the ports rather than to the harness: if
#    run 3 also prints the port's own lines, the switch does nothing and run 1
#    proves nothing about what was live.
one_run killswitch BANISHED_AUTOSTART=1 BANISHED_RT_DIFF=1 BANISHED_RT_DLGCREATE=0 \
                   BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=60
RC3=$?

echo "================ [ui3] SUMMARY ================"
echo "default rc=$RC1   armed rc=$RC2   killswitch rc=$RC3"
