#!/usr/bin/env bash
# tools/shadowre_cell.sh — Wave 10 lane SHADOWRE cell driver.
#
# Runs INSIDE `tools/game_dir_lock.sh run lane-SHADOWRE ...`, so the lock is
# already held. It does the two things a lane must never skip and which are easy
# to forget between cells:
#
#   1. re-`verify` ownership immediately before EVERY launch (not once at the top
#      — a 30-minute hold can be broken as stale between cell 1 and cell 4);
#   2. md5 the deployed exe against our own build immediately before EVERY
#      launch, and REFUSE on a mismatch rather than produce a plausible number
#      from somebody else's binary.
#
# Usage (from the worktree root):
#   tools/shadowre_cell.sh deploy
#   tools/shadowre_cell.sh cell <label> <timeout> -- VAR=VAL ...
#   tools/shadowre_cell.sh stress <label> <extra-env-as-one-string> -- <stress args>
set -u

GAME="${BANISHED_GAME_DIR:-C:/GOG Games/Banished}"
BUILD="standalone/build/reimpl-x64.exe"
EVID="docs/runtime_port/shadowre_logs"
mkdir -p "$EVID"

guard() {
    if ! tools/game_dir_lock.sh verify lane-SHADOWRE; then
        echo "[shadowre] LOCK NOT HELD — refusing to launch." >&2; exit 9
    fi
    local a b
    a=$(md5sum "$BUILD"            | cut -d' ' -f1)
    b=$(md5sum "$GAME/reimpl-x64.exe" | cut -d' ' -f1)
    echo "[shadowre] md5 built    = $a"
    echo "[shadowre] md5 deployed = $b"
    if [ "$a" != "$b" ]; then
        echo "[shadowre] MD5 MISMATCH — the deployed exe is not our build. REFUSING." >&2
        exit 9
    fi
}

case "${1:-}" in
deploy)
    if ! tools/game_dir_lock.sh verify lane-SHADOWRE; then exit 9; fi
    cp "$BUILD" "$GAME/reimpl-x64.exe" || exit 9
    echo "[shadowre] deployed; md5 $(md5sum "$GAME/reimpl-x64.exe" | cut -d' ' -f1)"
    ;;
cell)
    LABEL="$2"; TMO="$3"; shift 3; [ "${1:-}" = "--" ] && shift
    guard
    tools/rt_run.sh "$LABEL" --timeout "$TMO" -- "$@"
    RC=$?
    cp "$GAME/standalone_error.log" "$EVID/${LABEL}_error.log" 2>/dev/null
    cp "$GAME/standalone_crash.log" "$EVID/${LABEL}_crash.log" 2>/dev/null
    echo "[shadowre] cell $LABEL rc=$RC"
    ;;
stress)
    LABEL="$2"; shift 2; [ "${1:-}" = "--" ] && shift
    guard
    "$@"
    RC=$?
    cp "$GAME/standalone_error.log" "$EVID/${LABEL}_error.log" 2>/dev/null
    cp "$GAME/standalone_crash.log" "$EVID/${LABEL}_crash.log" 2>/dev/null
    echo "[shadowre] cell $LABEL rc=$RC"
    ;;
*) echo "usage: $0 deploy|cell|stress ..." >&2; exit 2 ;;
esac
