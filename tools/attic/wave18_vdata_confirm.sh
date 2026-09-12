#!/usr/bin/env bash
# tools/wave18_vdata_confirm.sh — one confirming boot with the eleven rows
# PROMOTED unconditionally (no BANISHED_RT_MINT_ROWS).
#
# The A/B pair proved the rows are safe and live when ARMED BY ENV. Registering
# them unconditionally is a different claim: it is what every future run does.
# This run is the check that the claim holds, and it carries the same screens so
# the numbers are comparable with the armed half.
set -u
OUT="${1:?usage: wave18_vdata_confirm.sh <outdir>}"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
GAME_DIR="${BANISHED_DIR:-C:/GOG Games/Banished}"

"$HERE/tools/game_dir_lock.sh" assert-held vdata || {
    echo "wave18_vdata_confirm: lock NOT held by vdata -- refusing to deploy."; exit 2; }

mkdir -p "$OUT"
cp -f "$HERE/standalone/build/reimpl-x64.exe" "$GAME_DIR/reimpl-x64.exe" || exit 1
echo "== deployed md5 $(md5sum "$GAME_DIR/reimpl-x64.exe" | cut -d' ' -f1)"
echo "== built    md5 $(md5sum "$HERE/standalone/build/reimpl-x64.exe" | cut -d' ' -f1)"

"$HERE/tools/rt_run.sh" promoted --timeout 900 -- \
    BANISHED_AUTOSTART=1 BANISHED_RT_DIFF=1 BANISHED_W17_ATTEND=1 \
    BANISHED_RT_MINT_SHADOW=1 BANISHED_RT_SAVE_TRACE=1 \
    BANISHED_FORCE_DEVICE_LOST=400 \
    BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=120
rc=$?
echo "== rt_run rc=$rc"
for f in standalone_debug.log standalone_error.log standalone_crash.log; do
    cp -f "$GAME_DIR/$f" "$OUT/promoted_$f" 2>/dev/null
done
ls -la "$OUT/promoted_standalone_crash.log"
exit $rc
