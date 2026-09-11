#!/usr/bin/env bash
# Wave 21 lane VFY — item 2: run file_stream_slots_probe.cpp (positive + negative
# controls) against the eight promoted BinaryFileStream bodies.
#
# Run ONLY under the lock:
#   tools/game_dir_lock.sh run lane-VFY 900 -- bash tools/lane_vfy_fsslots_run.sh [neg1|neg2]
set -u
G="C:/GOG Games/Banished"
SRC="$(cd "$(dirname "$0")/.." && pwd)"
MODE="${1:-1}"   # 1 = positive run, neg1/neg2 = negative controls

cp "$SRC/standalone/build/reimpl-x64.exe" "$G/reimpl-x64.exe" || exit 1
rm -f "$G/standalone_debug.log" "$G/standalone_error.log" "$G/standalone_crash.log"

cd "$SRC" || exit 1
tools/rt_run.sh "fsslots-$MODE" --timeout 300 -- \
  BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120 \
  BANISHED_RT_FSSLOTS_PROBE="$MODE"

echo "=== [FSSlots] lines ==="
grep -a "\[FSSlots\]" "$G/standalone_debug.log" "$G/standalone_error.log" 2>/dev/null
echo "=== crash log ==="
wc -c "$G/standalone_crash.log" 2>/dev/null || echo "no crash log"
