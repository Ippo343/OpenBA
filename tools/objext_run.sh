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

# ---- run A: PORTS ON, PROBE OFF ---------------------------------------------
# Runs first and on its own, because it answers the question the probe cannot:
# is the PORT stable in a live boot? The previous attempt crashed inside the
# probe's own negative control, and "the port is fine, my instrument was not"
# is a claim that needs its own run rather than an argument.
tools/rt_run.sh objextA --timeout 900 --     BANISHED_AUTOSTART=1     BANISHED_RT_DIFF=1     BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=60
echo "== objext_run: run A exit=$?"
cp -f "$GAME_DIR/standalone_error.log" "$OUT/A_error.log" 2>/dev/null
cp -f "$GAME_DIR/standalone_debug.log" "$OUT/A_debug.log" 2>/dev/null
cp -f "$GAME_DIR/standalone_crash.log" "$OUT/A_crash.log" 2>/dev/null

# ---- run C: PORTS ON, PROBE ON ----------------------------------------------
# Both screens, every control, one launch.
tools/rt_run.sh objextC --timeout 900 --     BANISHED_AUTOSTART=1     BANISHED_RT_DIFF=1     BANISHED_RT_PATHSTACK_PROBE=1     BANISHED_RT_COMPILED_SWEEP=1     BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=60
echo "== objext_run: run C exit=$?"
cp -f "$GAME_DIR/standalone_error.log" "$OUT/C_error.log" 2>/dev/null
cp -f "$GAME_DIR/standalone_debug.log" "$OUT/C_debug.log" 2>/dev/null
cp -f "$GAME_DIR/standalone_crash.log" "$OUT/C_crash.log" 2>/dev/null

# Run B (BANISHED_RT_PATHSTACK=0 BANISHED_RT_COMPILED=0, the kill switch) is NOT
# repeated here. It was measured under the previous hold and completed cleanly
# with an EMPTY crash log; the ports are OFF in it, so the binary change since
# then cannot affect its result. Its logs are lane_objext_logs/B_*.log. Dropping
# it shortens the hold, which is the scarce resource this wave.

echo "== objext_run: DONE"
