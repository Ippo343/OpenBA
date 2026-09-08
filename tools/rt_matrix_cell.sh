#!/usr/bin/env bash
# tools/rt_matrix_cell.sh — run ONE harness cell of the Wave 9 "differential
# oracle, full matrix" gate and archive its RAW logs.
#
# WHY a wrapper over rt_run.sh: the gate is recorded, not observed. After lane G
# the DLL is gone and no cell can be re-run, so each cell's evidence has to
# survive as a file — and it has to be the RAW log, because a truncated oracle
# report manufactures the `calls=0` signal the project reads ports on
# (reference_truncated_tool_output_manufactures_calls_zero.md).
#
# USAGE
#   tools/rt_matrix_cell.sh <cell-name> <timeout> -- VAR=VAL ...
#
# Archives to docs/runtime_port/wave9_lane_m/<cell-name>.{err,out}.log and
# prints the [RtDiff] ==== summary line plus every site with divergences>0.
set -u

CELL="${1:?usage: rt_matrix_cell.sh <cell> <timeout> -- ENV=VAL ...}"; shift
TIMEOUT="${1:?}"; shift
[ "${1:-}" = "--" ] && shift

REPO="$(cd "$(dirname "$0")/.." && pwd)"
GAME_DIR="${BANISHED_DIR:-C:/GOG Games/Banished}"
OUT="$REPO/docs/runtime_port/wave9_lane_m"
mkdir -p "$OUT"

"$REPO/tools/rt_run.sh" "$CELL" --timeout "$TIMEOUT" -- "$@" >"$OUT/$CELL.rt_run.txt" 2>&1
RC=$?

cp -f "$GAME_DIR/standalone_error.log" "$OUT/$CELL.err.log" 2>/dev/null
cp -f "$GAME_DIR/standalone_debug.log" "$OUT/$CELL.out.log" 2>/dev/null
if [ -s "$GAME_DIR/standalone_crash.log" ]; then
    cp -f "$GAME_DIR/standalone_crash.log" "$OUT/$CELL.crash.log"
fi

echo "=== cell=$CELL rc=$RC ==="
head -2 "$OUT/$CELL.rt_run.txt"
echo "--- RtDiff summary (raw, untruncated) ---"
grep -a '\[RtDiff\] ====' "$OUT/$CELL.err.log" 2>/dev/null || echo "(NO SUMMARY — process did not exit cleanly)"
echo "--- sites with divergences>0 ---"
grep -a '\[RtDiff\]   ' "$OUT/$CELL.err.log" 2>/dev/null | grep -av 'divergences=0$' || echo "(none)"
echo "--- site call totals ---"
grep -ac '\[RtDiff\]   ' "$OUT/$CELL.err.log" 2>/dev/null
echo "--- crash ---"
[ -f "$OUT/$CELL.crash.log" ] && head -12 "$OUT/$CELL.crash.log" || echo "(none)"
exit $RC
