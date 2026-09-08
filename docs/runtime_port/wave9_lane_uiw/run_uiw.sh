#!/usr/bin/env bash
# docs/runtime_port/wave9_lane_uiw/run_uiw.sh — Wave 9 lane UIW's runner.
#
# WHY NOT tools/rt_run.sh: six lanes deploy to the ONE shared game directory as
# `reimpl-x64.exe`. Twice in this lane a concurrent deploy replaced the exe
# between our `cp` and our launch, and the second time it was only caught by an
# md5 check after the fact — the run had already produced a full, plausible,
# WRONG diff summary from somebody else's binary.
#
# So this runner deploys under a LANE-UNIQUE NAME (`reimpl-uiw.exe`), which no
# other lane writes, and re-checks the md5 after the run. The logs are still
# shared (the exe opens them by fixed name), so it also refuses to start while
# any reimpl*.exe is alive and reports if one appears during the run.
#
# usage: run_uiw.sh <label> <timeout-seconds> ENV=VAL ...
set -u
GAME_DIR="${BANISHED_DIR:-C:/GOG Games/Banished}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
SRC="$ROOT/standalone/build/reimpl-x64.exe"
DST="$GAME_DIR/reimpl-uiw.exe"

LABEL="${1:?label}"; shift
TIMEOUT="${1:?timeout}"; shift

[ -f "$SRC" ] || { echo "run_uiw: no built exe at $SRC"; exit 2; }

for i in $(seq 1 120); do
    tasklist 2>/dev/null | grep -qi 'reimpl' || break
    [ "$i" = 1 ] && echo "run_uiw[$LABEL]: waiting for another lane's run to finish..."
    sleep 10
done
if tasklist 2>/dev/null | grep -qi 'reimpl'; then
    echo "run_uiw[$LABEL]: another reimpl is STILL running after 20min — refusing"; exit 2
fi

cp "$SRC" "$DST" || exit 2
WANT="$(md5sum "$SRC" | cut -d' ' -f1)"
GOT="$(md5sum "$DST" | cut -d' ' -f1)"
[ "$WANT" = "$GOT" ] || { echo "run_uiw[$LABEL]: deploy md5 mismatch"; exit 2; }
echo "run_uiw[$LABEL]: deployed $WANT"

# The logs are shared and are held open by whoever is running. If we cannot
# delete them, someone else's process owns them RIGHT NOW and anything we
# measure is theirs — so this is a hard gate, not a warning. It is also the only
# reliable detector: a lane can start its process between our tasklist check and
# our launch, and it did, twice.
for i in $(seq 1 180); do
    rm -f "$GAME_DIR/standalone_debug.log" "$GAME_DIR/standalone_error.log" \
          "$GAME_DIR/standalone_crash.log" 2>/dev/null
    [ -f "$GAME_DIR/standalone_debug.log" ] || break
    [ "$i" = 1 ] && echo "run_uiw[$LABEL]: logs are locked by another run — waiting"
    sleep 10
done
if [ -f "$GAME_DIR/standalone_debug.log" ]; then
    echo "run_uiw[$LABEL]: logs STILL locked — refusing to measure someone else's run"; exit 2
fi

( cd "$GAME_DIR" && env "$@" ./reimpl-uiw.exe ) &
RUNPID=$!

"$ROOT/tools/boot_state.sh" --wait 240 >/dev/null 2>&1
if [ "$?" = "2" ]; then
    echo "run_uiw[$LABEL]: BLOCKED_DIALOG"
    taskkill //F //IM reimpl-uiw.exe >/dev/null 2>&1; exit 3
fi

FOREIGN=0
WAITED=0
while kill -0 "$RUNPID" 2>/dev/null; do
    sleep 5; WAITED=$((WAITED+5))
    tasklist 2>/dev/null | grep -i 'reimpl' | grep -qvi 'reimpl-uiw' && FOREIGN=1
    if [ "$WAITED" -ge "$TIMEOUT" ]; then
        echo "run_uiw[$LABEL]: TIMEOUT after ${TIMEOUT}s"
        taskkill //F //IM reimpl-uiw.exe >/dev/null 2>&1; break
    fi
done
wait "$RUNPID" 2>/dev/null

cp "$GAME_DIR/standalone_error.log" "$HERE/${LABEL}_error.log" 2>/dev/null
cp "$GAME_DIR/standalone_debug.log" "$HERE/${LABEL}_debug.log" 2>/dev/null
cp "$GAME_DIR/standalone_crash.log" "$HERE/${LABEL}_crash.log" 2>/dev/null

AFTER="$(md5sum "$DST" 2>/dev/null | cut -d' ' -f1)"
echo "run_uiw[$LABEL]: exe md5 after = $AFTER (want $WANT)"
[ "$AFTER" = "$WANT" ] || echo "run_uiw[$LABEL]: *** THE EXE CHANGED UNDER US — DISCARD THIS RUN ***"
[ "$FOREIGN" = 0 ] || echo "run_uiw[$LABEL]: *** ANOTHER reimpl RAN CONCURRENTLY — the shared logs may be mixed ***"
echo "run_uiw[$LABEL]: logs in $HERE/${LABEL}_*.log"
