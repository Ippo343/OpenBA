#!/usr/bin/env bash
# tools/laneN_bracket.sh — Wave 9 lane N, Finding 3: bracket the writer.
# Repeats the stress cell with the per-step boundary-tag scan until a run either
# reports the step that dirtied the heap or faults, then archives it.
set -u
REPO="$(cd "$(dirname "$0")/.." && pwd)"
GAME="C:/GOG Games/Banished"
OUT="$REPO/docs/runtime_port/wave9_lane_n"
mkdir -p "$OUT"
N="${1:-10}"
EVERY="${2:-16}"

for i in $(seq 1 "$N"); do
    "$REPO/tools/rt_run.sh" "laneN_b$i" --timeout 900 -- \
        BANISHED_STRESS=1 BANISHED_STRESS_FRAMES=1200 \
        BANISHED_WORKSCHED_PROBE=1 BANISHED_TEARDOWN_BRACKET="$EVERY" \
        > "/tmp/laneN_b$i.txt" 2>&1
    rc=$?
    hit=$(grep -ac 'HEAP WENT CORRUPT' "$GAME/standalone_error.log" 2>/dev/null)
    echo "== run $i rc=$rc bracketHit=$hit"
    grep -a 'TeardownBracket\]' "$GAME/standalone_error.log" 2>/dev/null | head -4 | sed 's/^/   /'
    if [ "$hit" != "0" ] || [ -s "$GAME/standalone_crash.log" ]; then
        echo "== ARCHIVING run $i"
        cp -f "$GAME/standalone_error.log" "$OUT/finding3_bracket.err.log"
        cp -f "$GAME/standalone_debug.log" "$OUT/finding3_bracket.out.log" 2>/dev/null
        [ -s "$GAME/standalone_crash.log" ] && cp -f "$GAME/standalone_crash.log" "$OUT/finding3_bracket.crash.log"
        exit 0
    fi
done
echo "== nothing in $N runs"
exit 1
