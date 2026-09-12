#!/usr/bin/env bash
# tools/laneN_writer.sh — Wave 9 lane N, Finding 3: name the WRITER.
#
# The heap is provably intact at teardown entry and corrupt by the fault, so the
# store happens during teardown. This repeats the stress cell with the inner-tree
# validator plus the hardware write watchpoint until either a corrupt link is
# named or a run faults, and archives whichever happens.
set -u
REPO="$(cd "$(dirname "$0")/.." && pwd)"
GAME="C:/GOG Games/Banished"
OUT="$REPO/docs/runtime_port/wave9_lane_n"
mkdir -p "$OUT"
N="${1:-8}"

for i in $(seq 1 "$N"); do
    "$REPO/tools/rt_run.sh" "laneN_w$i" --timeout 900 -- \
        BANISHED_STRESS=1 BANISHED_STRESS_FRAMES=1200 \
        BANISHED_WORKSCHED_PROBE=1 BANISHED_WSWATCH=1 > "/tmp/laneN_w$i.txt" 2>&1
    rc=$?
    hit=$(grep -ac 'WSWatch\] \*\*\* CORRUPT LINK' "$GAME/standalone_error.log" 2>/dev/null)
    dr=$(grep -ac 'HwWatch' "$GAME/standalone_error.log" 2>/dev/null)
    echo "== run $i rc=$rc corruptLinks=$hit hwwatchLines=$dr"
    grep -a 'WSWatch\]' "$GAME/standalone_error.log" 2>/dev/null | head -6 | sed 's/^/   /'
    grep -a 'WorkSchedProbe\] TALLY' "$GAME/standalone_error.log" 2>/dev/null | sed 's/^/   /'
    if [ "$hit" != "0" ] || [ -s "$GAME/standalone_crash.log" ]; then
        echo "== ARCHIVING run $i"
        cp -f "$GAME/standalone_error.log" "$OUT/finding3_writer.err.log"
        cp -f "$GAME/standalone_debug.log" "$OUT/finding3_writer.out.log" 2>/dev/null
        [ -s "$GAME/standalone_crash.log" ] && cp -f "$GAME/standalone_crash.log" "$OUT/finding3_writer.crash.log"
        exit 0
    fi
done
echo "== nothing in $N runs"
exit 1
