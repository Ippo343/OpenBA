#!/usr/bin/env bash
# tools/rt_run.sh — run the already-deployed standalone exe under a chosen env and
# wait for it to exit, then report the Runtime-port lines from the logs.
#
# WHY: every runtime-port verification is the same shape — launch with a specific
# BANISHED_RT_* combination, let the harness drive itself to a clean exit, then read
# [RtPort] / [RtDiff] / [RtDetour] out of standalone_error.log. Doing that by hand
# invites the classic traps: polling a BLOCKED boot until the timeout expires, and
# measuring a stale exe.
#
# USAGE
#   tools/rt_run.sh <label> [--timeout N] -- VAR=VAL VAR=VAL ...
# e.g.
#   tools/rt_run.sh diff --timeout 900 -- BANISHED_AUTOSTART=1 BANISHED_RT_DIFF=1 \
#                                          BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=60
#
# Exit code 0 = process exited on its own within the timeout. Non-zero otherwise.
set -u

GAME_DIR="${BANISHED_DIR:-C:/GOG Games/Banished}"
LABEL="${1:?usage: rt_run.sh <label> [--timeout N] -- ENV=VAL ...}"; shift
TIMEOUT=900
if [ "${1:-}" = "--timeout" ]; then TIMEOUT="$2"; shift 2; fi
[ "${1:-}" = "--" ] && shift

EXE="$GAME_DIR/reimpl-x64.exe"
if [ ! -f "$EXE" ]; then echo "rt_run: no deployed exe at $EXE"; exit 2; fi
echo "== rt_run[$LABEL] exe stamp: $(stat -c %y "$EXE" 2>/dev/null || date -r "$EXE")"

# Any leftover process holds the exe and the logs (see the zombie note in memory).
if tasklist 2>/dev/null | grep -qi 'reimpl-x64\.exe'; then
    echo "rt_run: a reimpl-x64.exe is ALREADY running — refusing to launch a second."
    exit 2
fi

rm -f "$GAME_DIR/standalone_debug.log" "$GAME_DIR/standalone_error.log" \
      "$GAME_DIR/standalone_crash.log"

( cd "$GAME_DIR" && env "$@" ./reimpl-x64.exe ) &
RUNPID=$!

# Fail fast on a modal dialog rather than burning the whole timeout on a blocked boot.
tools/boot_state.sh --wait 240 >/dev/null 2>&1
BOOT=$?
if [ "$BOOT" = "2" ]; then
    echo "rt_run[$LABEL]: BLOCKED_DIALOG — boot will never proceed unattended"
    taskkill //F //IM reimpl-x64.exe >/dev/null 2>&1
    exit 3
fi

WAITED=0
while kill -0 "$RUNPID" 2>/dev/null; do
    sleep 5; WAITED=$((WAITED+5))
    if [ "$WAITED" -ge "$TIMEOUT" ]; then
        echo "rt_run[$LABEL]: TIMEOUT after ${TIMEOUT}s — killing"
        taskkill //F //IM reimpl-x64.exe >/dev/null 2>&1
        wait "$RUNPID" 2>/dev/null
        RC=124
        break
    fi
done
RC="${RC:-0}"
[ "$RC" = "0" ] && { wait "$RUNPID" 2>/dev/null; RC=$?; }

echo "== rt_run[$LABEL] exited rc=$RC after ${WAITED}s"
echo "-- crash log --"
if [ -s "$GAME_DIR/standalone_crash.log" ]; then head -30 "$GAME_DIR/standalone_crash.log"; else echo "(none)"; fi
echo "-- runtime port lines --"
grep -a -E '\[RtPort\]|\[RtDiff\]|\[RtDetour\]|\[RtCensus\]' \
     "$GAME_DIR/standalone_error.log" "$GAME_DIR/standalone_debug.log" 2>/dev/null | tail -80
exit "$RC"
