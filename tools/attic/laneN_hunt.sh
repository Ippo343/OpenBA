#!/usr/bin/env bash
# tools/laneN_hunt.sh — Wave 9 lane N, Finding 3.
#
# The teardown fault is intermittent (roughly 4 runs in 10 on the stress route),
# so a single clean run says nothing about it. This repeats the stress cell with
# BOTH diagnostics live until one run faults, then archives that run's raw logs.
# Both tallies now print from the crash handler as well as from atexit, because
# the first version of the probe registered only with atexit and the one run
# where it fired printed nothing (ExitProcess skips atexit).
set -u
REPO="$(cd "$(dirname "$0")/.." && pwd)"
GAME="C:/GOG Games/Banished"
OUT="$REPO/docs/runtime_port/wave9_lane_n"
mkdir -p "$OUT"
N="${1:-6}"

for i in $(seq 1 "$N"); do
    "$REPO/tools/rt_run.sh" "laneN_hunt$i" --timeout 900 -- \
        BANISHED_STRESS=1 BANISHED_STRESS_FRAMES=1200 \
        BANISHED_WORKSCHED_PROBE=1 BANISHED_OBJAUDIT=1 > "/tmp/laneN_hunt$i.txt" 2>&1
    rc=$?
    probe=$(grep -a 'WorkSchedProbe\] TALLY' "$GAME/standalone_error.log" 2>/dev/null | tail -1)
    audit=$(grep -a 'ObjAudit\] post-entitylist: count' "$GAME/standalone_error.log" 2>/dev/null | tail -1)
    echo "== run $i rc=$rc"
    echo "   $probe"
    echo "   $audit"
    if [ -s "$GAME/standalone_crash.log" ]; then
        echo "== CRASHED on run $i — archiving"
        cp -f "$GAME/standalone_error.log" "$OUT/finding3_crashrun.err.log"
        cp -f "$GAME/standalone_crash.log" "$OUT/finding3_crashrun.crash.log"
        cp -f "/tmp/laneN_hunt$i.txt"      "$OUT/finding3_crashrun.rt_run.txt"
        exit 0
    fi
done
echo "== no fault in $N runs"
exit 1
