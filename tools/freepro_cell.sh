#!/usr/bin/env bash
# =============================================================================
# tools/freepro_cell.sh -- Wave 10 lane FREEPRO: run one measurement cell
# =============================================================================
#
# ALWAYS invoke through the lock, never bare:
#
#   tools/game_dir_lock.sh run lane-FREEPRO 900 -- \
#       tools/freepro_cell.sh <label> [VAR=VALUE ...]
#
# `run` acquires, checks the status itself, runs, and releases -- which is the
# fix for the pipe that swallowed an `acquire` failure and clobbered another
# lane's exe on 2026-09-08.
#
# This script additionally md5s the deployed exe against our own build
# IMMEDIATELY BEFORE LAUNCH and refuses to run on a mismatch. The lock is
# advisory; the md5 is the only thing that can actually prove the bytes about
# to run are the bytes we built. This lane's whole question is a register-level
# one about an armed allocator, and a cell measured against another lane's
# binary would answer it wrongly in either direction.
# =============================================================================
set -u
GAME="${BANISHED_GAME_DIR:-C:/GOG Games/Banished}"
SRC=standalone/build/reimpl-x64.exe
LABEL="$1"; shift
OUT="perf/freepro/$LABEL"
mkdir -p "$OUT"

if ! tools/game_dir_lock.sh verify lane-FREEPRO; then
    echo "[freepro] we do NOT hold the lock -- refusing to deploy" >&2
    exit 1
fi

MINE=$(md5sum "$SRC" | cut -d' ' -f1)
cp "$SRC" "$GAME/reimpl-x64.exe" || exit 1
THEIRS=$(md5sum "$GAME/reimpl-x64.exe" | cut -d' ' -f1)
echo "[freepro] md5 built   = $MINE"
echo "[freepro] md5 deployed= $THEIRS"
if [ "$MINE" != "$THEIRS" ]; then
    echo "[freepro] MD5 MISMATCH -- the deployed exe is not ours. Refusing." >&2
    exit 1
fi
echo "$MINE  $LABEL  $(date -Iseconds)" >> "$OUT/md5.txt"

rm -f "$GAME"/standalone_debug.log "$GAME"/standalone_error.log \
      "$GAME"/standalone_crash.log "$GAME"/standalone_boot_state.txt

(
  cd "$GAME" || exit 1
  for kv in "$@"; do export "$kv"; done
  ./reimpl-x64.exe
)
echo "[freepro] exe exit=$?"

# Re-check ownership AFTER the run: if we lost the lock mid-run the logs below
# may not be ours, and a silently-foreign log is exactly this wave's failure mode.
if ! tools/game_dir_lock.sh verify lane-FREEPRO; then
    echo "[freepro] WARNING: lock was NOT ours after the run -- logs are SUSPECT" >&2
fi
for f in standalone_debug.log standalone_error.log standalone_crash.log; do
    [ -f "$GAME/$f" ] && cp "$GAME/$f" "$OUT/"
done
echo "[freepro] logs -> $OUT"
