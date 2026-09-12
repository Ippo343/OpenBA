#!/usr/bin/env bash
# tools/laneN_hunt2.sh — Wave 9 lane N, Finding 3: ONE latent write, or several
# independent teardown faults?
#
# Three different stacks have been recorded for this fault. Both readings are
# consistent with that, and they differ in exactly one observable: whether the
# engine heap is ALREADY corrupt when teardown begins. This repeats the stress
# cell with the boundary-tag walk bracketing teardown until a run faults, and
# archives it.
set -u
REPO="$(cd "$(dirname "$0")/.." && pwd)"
GAME="C:/GOG Games/Banished"
OUT="$REPO/docs/runtime_port/wave9_lane_n"
mkdir -p "$OUT"
N="${1:-8}"

for i in $(seq 1 "$N"); do
    "$REPO/tools/rt_run.sh" "laneN_hw$i" --timeout 900 -- \
        BANISHED_STRESS=1 BANISHED_STRESS_FRAMES=1200 \
        BANISHED_WORKSCHED_PROBE=1 BANISHED_OBJAUDIT=1 \
        BANISHED_TEARDOWN_HEAPWALK=1 > "/tmp/laneN_hw$i.txt" 2>&1
    rc=$?
    echo "== run $i rc=$rc"
    grep -a 'REDZONE-DUMP.*shutdown-enter\|REDZONE-DUMP.*post-entitylist' \
        "$GAME/standalone_error.log" 2>/dev/null | head -4 | sed 's/^/   /'
    grep -a "'shutdown-enter'\|'post-entitylist'" "$GAME/standalone_debug.log" 2>/dev/null \
        | head -4 | sed 's/^/   /'
    if [ -s "$GAME/standalone_crash.log" ]; then
        echo "== CRASHED on run $i — archiving"
        cp -f "$GAME/standalone_error.log" "$OUT/finding3_heapwalk.err.log"
        cp -f "$GAME/standalone_debug.log" "$OUT/finding3_heapwalk.out.log"
        cp -f "$GAME/standalone_crash.log" "$OUT/finding3_heapwalk.crash.log"
        exit 0
    fi
done
echo "== no fault in $N runs"
exit 1
