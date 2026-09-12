#!/usr/bin/env bash
# tools/wave25_alloc_cell.sh -- WAVE 25 lane ALLOC cell driver.
#
# Same shape as tools/shadowbisect_cell.sh (Wave 10) but keyed to THIS lane's lock
# name, because a cell driver that verifies somebody else's lock name is a guard that
# cannot fire. Re-verifies the lock and md5s the deployed exe against our own build
# before EVERY launch -- a long hold can be broken as stale between cells, and Wave 9
# recorded a measurement silently taken against another lane's binary.
set -u
GAME="${BANISHED_GAME_DIR:-C:/GOG Games/Banished}"
BUILD="${W25_BUILD:-standalone/build/reimpl-x64.exe}"
EVID="docs/runtime_port/wave25_alloc_logs"
LANE="lane-ALLOC25"
mkdir -p "$EVID"

guard() {
    tools/game_dir_lock.sh verify "$LANE" || { echo "[w25alloc] LOCK NOT HELD -- refusing." >&2; exit 9; }
    local a b
    a=$(md5sum "$BUILD" | cut -d' ' -f1)
    b=$(md5sum "$GAME/reimpl-x64.exe" | cut -d' ' -f1)
    echo "[w25alloc] md5 built=$a deployed=$b"
    [ "$a" = "$b" ] || { echo "[w25alloc] MD5 MISMATCH -- REFUSING." >&2; exit 9; }
}

case "${1:-}" in
deploy)
    tools/game_dir_lock.sh verify "$LANE" || exit 9
    cp "$BUILD" "$GAME/reimpl-x64.exe" || exit 9
    echo "[w25alloc] deployed md5 $(md5sum "$GAME/reimpl-x64.exe" | cut -d' ' -f1)"
    ;;
cell)
    LABEL="$2"; TMO="$3"; shift 3; [ "${1:-}" = "--" ] && shift
    guard
    tools/rt_run.sh "$LABEL" --timeout "$TMO" -- "$@"
    RC=$?
    cp "$GAME/standalone_error.log" "$EVID/${LABEL}_error.log" 2>/dev/null
    cp "$GAME/standalone_crash.log" "$EVID/${LABEL}_crash.log" 2>/dev/null
    echo "[w25alloc] cell $LABEL rc=$RC"
    ;;
*) echo "usage: $0 deploy|cell <label> <timeout> -- ENV=VAL ..." >&2; exit 2 ;;
esac
