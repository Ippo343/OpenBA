#!/usr/bin/env bash
# Lane TYPE baseline: the SAME survey with Runtime-x64.dll PRESENT and the mint
# off. The difference between this run's node count and the cutover run's is
# exactly what the mint fails to reproduce.
set -u
G="/c/GOG Games/Banished"
OUT="${1:?usage: lane_type_run_dll.sh <outdir>}"
mkdir -p "$OUT"

cp -f standalone/build/reimpl-x64.exe "$G/reimpl-x64.exe"
rm -f "$G/standalone_debug.log" "$G/standalone_error.log" "$G/standalone_crash.log"

cd "$G"
BANISHED_RT_TYPE_SURVEY=1 BANISHED_AUTOSTART=1 \
BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120 \
./reimpl-x64.exe &
PID=$!
for i in $(seq 1 60); do
  kill -0 $PID 2>/dev/null || break
  sleep 2
done
kill -9 $PID 2>/dev/null
wait $PID 2>/dev/null
cd - >/dev/null

for f in standalone_debug.log standalone_error.log standalone_crash.log; do
  [ -f "$G/$f" ] && cp -f "$G/$f" "$OUT/$f"
done
echo "[laneTYPE] baseline logs copied to $OUT"
