#!/usr/bin/env bash
# Wave 10E lane FINAL — deploy this worktree's debug exe and run ONE route,
# inside a single lock hold. Usage:
#   ./lane_final_run.sh <label> KEY=VAL [KEY=VAL ...]
# The md5 of the deployed exe is printed on both sides of the run, so a run
# taken against another lane's binary cannot pass unnoticed.
set -u
LABEL="$1"; shift
GAME="C:/GOG Games/Banished"
SRC="$(pwd)/standalone/build/reimpl-x64.exe"

echo "== $LABEL =="
cp -f "$SRC" "$GAME/reimpl-x64.exe" || { echo "DEPLOY FAILED"; exit 1; }
md5sum "$GAME/reimpl-x64.exe"

rm -f "$GAME/standalone_debug.log" "$GAME/standalone_error.log" "$GAME/standalone_crash.log"

( cd "$GAME" && env "$@" ./reimpl-x64.exe ) &
PID=$!
for i in $(seq 1 150); do
    sleep 2
    kill -0 $PID 2>/dev/null || break
done
if kill -0 $PID 2>/dev/null; then
    echo "TIMEOUT -- killing"
    kill -9 $PID 2>/dev/null
fi
wait $PID 2>/dev/null
md5sum "$GAME/reimpl-x64.exe"
echo "== $LABEL done =="
