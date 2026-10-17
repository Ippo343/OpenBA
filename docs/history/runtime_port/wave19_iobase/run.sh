#!/usr/bin/env bash
# Wave 19 lane IOBASE -- deploy + run. Called UNDER the lock by
# tools/game_dir_lock.sh run, so it must not acquire anything itself.
set -u
GD="/c/GOG Games/Banished"
WT="/c/code/GIT/wt-w19-iobase"
OUT="$1"; shift
case "$OUT" in /*) ;; *) OUT="$WT/$OUT" ;; esac

"$WT/tools/game_dir_lock.sh" assert-held lane-IOBASE || exit 9

cp "$WT/standalone/build/reimpl-x64.exe" "$GD/reimpl-x64.exe" || exit 8
rm -f "$GD/standalone_debug.log" "$GD/standalone_error.log" "$GD/standalone_crash.log"

cd "$GD" || exit 7
env "$@" ./reimpl-x64.exe &
PID=$!
"$WT/tools/boot_state.sh" --wait 300
echo "[iobase] boot_state exit=$?"
for i in $(seq 1 240); do
  kill -0 "$PID" 2>/dev/null || break
  sleep 2
done
kill -0 "$PID" 2>/dev/null && { echo "[iobase] STILL RUNNING after 480s -- killing"; kill -9 "$PID"; }
wait "$PID"; echo "[iobase] exe exit=$?"

mkdir -p "$OUT"
cp "$GD/standalone_debug.log" "$OUT/" 2>/dev/null
cp "$GD/standalone_error.log" "$OUT/" 2>/dev/null
cp "$GD/standalone_crash.log" "$OUT/" 2>/dev/null
ls -l "$OUT"
