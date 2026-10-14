#!/usr/bin/env bash
# Load census: run one route with the perf exe deployed, timestamp every stderr line
# (ms since launch) and keep the sampler output.
# usage: census.sh <route: menu|new|load> <outdir> [sampleHz]
ROUTE=$1; OUT=$2; HZ=${3:-1000}; KIT="$(cd "$(dirname "$0")" && pwd)"
G="/c/GOG Games/Banished"
mkdir -p "$OUT"
cd "$G" || exit 2
rm -f standalone_debug.log standalone_error.log standalone_crash.log standalone_boot_state.txt standalone_samples.txt
case $ROUTE in
  menu) ENVS="BANISHED_AUTOQUIT=1";;
  new)  ENVS="BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=300 BANISHED_PAUSEMENU_QUIT=120";;
  load) ENVS="BANISHED_MENU_SCENE=0 BANISHED_AUTOLOAD=Gate BANISHED_PAUSEMENU=300 BANISHED_PAUSEMENU_QUIT=120";;
esac
[ "$HZ" != 0 ] && ENVS="$ENVS BANISHED_SAMPLE=$HZ"
T0=$(date +%s%N)
env $ENVS MSYS2_ARG_CONV_EXCL="/profile:" ./reimpl-x64.exe /profile:faithful &
PID=$!
WPID=$(cat /proc/$PID/winpid)
python "$KIT/stamp.py" "$G/standalone_error.log" "$(cygpath -w "$OUT")/stamped_error.log" $T0 $WPID &
W=$!
for i in $(seq 1 1800); do kill -0 $PID 2>/dev/null || break; sleep 0.1; done
kill -0 $PID 2>/dev/null && { echo "HANG, killing"; kill -9 $PID; }
wait $PID; RC=$?
T1=$(date +%s%N)
wait $W
echo "route=$ROUTE rc=$RC wall_ms=$(( (T1 - T0) / 1000000 ))" | tee "$OUT/summary.txt"
cp -f standalone_error.log standalone_debug.log standalone_crash.log standalone_samples.txt "$OUT/" 2>/dev/null
