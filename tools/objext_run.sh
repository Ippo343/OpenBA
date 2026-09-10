#!/usr/bin/env bash
# tools/objext_run.sh — Wave 11 lane OBJEXT's whole verification under ONE lock hold.
#
# Three lanes share one game directory this wave and a blocking acquire has
# already timed out once, so every run this lane needs is batched under a single
# hold and the hold is released immediately afterwards.
#
# TWO TRAPS THIS SCRIPT IS WRITTEN AROUND:
#   1. `acquire`'s exit status must be read DIRECTLY. A pipeline returns the last
#      command's status and a wrapping `if`/function returns its own, so a
#      TIMEOUT reads as a GRANT either way. `rc=$?` on the very next line is the
#      only spelling that cannot lie.
#   2. The deployed exe is whatever the last lane left there. Its md5 is compared
#      against the build under test BEFORE any run, because a green measurement
#      of somebody else's binary is worse than no measurement.
set -u

LANE=objext
GAME_DIR="${BANISHED_DIR:-C:/GOG Games/Banished}"
MINE=standalone/build/reimpl-x64.exe
OUT=lane_objext_logs
mkdir -p "$OUT"

[ -f "$MINE" ] || { echo "objext_run: no build at $MINE"; exit 2; }
WANT=$(md5sum "$MINE" | cut -d' ' -f1)
echo "== objext_run: build under test md5=$WANT"

tools/game_dir_lock.sh acquire "$LANE"
rc=$?
if [ "$rc" != 0 ]; then
    echo "objext_run: ACQUIRE FAILED rc=$rc -- NOT deploying, NOT running."
    exit 9
fi
echo "== objext_run: lock GRANTED"

cleanup() { tools/game_dir_lock.sh release "$LANE" >/dev/null 2>&1; echo "== objext_run: lock released"; }
trap cleanup EXIT

cp -f "$MINE" "$GAME_DIR/reimpl-x64.exe" || { echo "objext_run: deploy failed"; exit 3; }
GOT=$(md5sum "$GAME_DIR/reimpl-x64.exe" | cut -d' ' -f1)
if [ "$GOT" != "$WANT" ]; then
    echo "objext_run: DEPLOYED md5=$GOT != BUILT md5=$WANT -- refusing to measure"
    exit 4
fi
echo "== objext_run: deployed md5 matches"

# ---- run A: everything armed, one launch ------------------------------------
# The probe and the sweep both self-schedule off FindOrLoad's first call, so a
# single menu-to-gameplay run drives every control of both screens.
tools/rt_run.sh objextA --timeout 900 -- \
    BANISHED_AUTOSTART=1 \
    BANISHED_RT_DIFF=1 \
    BANISHED_RT_PATHSTACK_PROBE=1 \
    BANISHED_RT_COMPILED_SWEEP=1 \
    BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=60
echo "== objext_run: run A exit=$?"
cp -f "$GAME_DIR/standalone_error.log" "$OUT/A_error.log" 2>/dev/null
cp -f "$GAME_DIR/standalone_debug.log" "$OUT/A_debug.log" 2>/dev/null
cp -f "$GAME_DIR/standalone_crash.log" "$OUT/A_crash.log" 2>/dev/null

# ---- run B: the kill switch --------------------------------------------------
# BANISHED_RT_PATHSTACK=0 / BANISHED_RT_COMPILED=0 leave all four rows on the
# DLL. This is the A/B that says the ports are what ran in A, rather than the
# DLL running in both and the instruments agreeing about it.
tools/rt_run.sh objextB --timeout 900 -- \
    BANISHED_AUTOSTART=1 \
    BANISHED_RT_PATHSTACK=0 BANISHED_RT_COMPILED=0 \
    BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=60
echo "== objext_run: run B exit=$?"
cp -f "$GAME_DIR/standalone_error.log" "$OUT/B_error.log" 2>/dev/null
cp -f "$GAME_DIR/standalone_debug.log" "$OUT/B_debug.log" 2>/dev/null
cp -f "$GAME_DIR/standalone_crash.log" "$OUT/B_crash.log" 2>/dev/null

echo "== objext_run: DONE"
