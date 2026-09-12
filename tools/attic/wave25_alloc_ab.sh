#!/usr/bin/env bash
# tools/wave25_alloc_ab.sh -- WAVE 25 lane ALLOC counterbalanced perf A/B driver.
#
# WHY IT IS NOT A LOOP OF `stress_run.sh --label on/off`
#
# Wave 22's A/B alternated arms but ran ON FIRST IN EVERY PAIR. A confounder that
# RISES during the session (a sibling build, a lane opening) then lands more heavily
# on the second run of each pair, and a fixed within-pair order converts that drift
# into an apparent treatment effect -- it produced a spurious "arming is 20% faster".
# Alternating is NOT enough; the ORDER must be counterbalanced too.
#
# So this driver takes the arm order as an explicit argument and the caller runs
# ON,OFF and OFF,ON pairs in equal numbers.
#
#   tools/wave25_alloc_ab.sh <pair-index> <first-arm: on|off>
#
# Each run re-verifies the game-dir lock and md5s the deployed exe (a long hold can be
# broken as stale mid-series, and Wave 9 recorded a measurement silently taken against
# another lane's binary). ONE binary, ONE env var -- never two builds.
set -u
cd "$(dirname "$0")/.." || exit 1
GAME="${BANISHED_GAME_DIR:-C:/GOG Games/Banished}"
BUILD="standalone/build-perf/reimpl-x64.exe"
LANE="lane-ALLOC25"
PAIR="${1:?usage: wave25_alloc_ab.sh <pair-index> <on|off>}"
FIRST="${2:?usage: wave25_alloc_ab.sh <pair-index> <on|off>}"

run_arm() {
    local arm="$1"
    tools/game_dir_lock.sh verify "$LANE" || { echo "[w25ab] LOCK NOT HELD -- refusing." >&2; exit 9; }
    local a b
    a=$(md5sum "$BUILD" | cut -d' ' -f1)
    b=$(md5sum "$GAME/reimpl-x64.exe" | cut -d' ' -f1)
    [ "$a" = "$b" ] || { echo "[w25ab] MD5 MISMATCH built=$a deployed=$b -- REFUSING." >&2; exit 9; }

    # The arm is applied HERE and nowhere else, so the two arms differ by exactly one
    # environment variable on one binary.
    if [ "$arm" = "on" ]; then export BANISHED_RT_ALLOC=1; else unset BANISHED_RT_ALLOC; fi
    echo "== pair $PAIR arm=$arm (BANISHED_RT_ALLOC=${BANISHED_RT_ALLOC:-unset}) md5=$a"
    tools/stress_run.sh --label "w25ab_p${PAIR}_${arm}" --no-build
    local rc=$?
    echo "== pair $PAIR arm=$arm rc=$rc"
    return 0
}

if [ "$FIRST" = "on" ]; then run_arm on; run_arm off; else run_arm off; run_arm on; fi
