#!/usr/bin/env bash
# Wave 19 lane MINT: the readiness/trap screen for the five newly minted IO
# vtable rows, on the regenerated mint.
#
# Run ONLY under the lock, as its own statement:
#   tools/game_dir_lock.sh run lane-MINT 900 -- bash tools/lane_mint_shadow_run.sh
set -u
G="C:/GOG Games/Banished"
SRC="$(cd "$(dirname "$0")/.." && pwd)"

cp "$SRC/standalone/build/reimpl-x64.exe" "$G/reimpl-x64.exe" || exit 1
rm -f "$G/standalone_debug.log" "$G/standalone_error.log" "$G/standalone_crash.log"

cd "$G" || exit 1
BANISHED_RT_MINT_SHADOW=1 BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 \
  BANISHED_PAUSEMENU_QUIT=120 ./reimpl-x64.exe
echo "EXE EXIT=$?"

echo "=== summary lines ==="
grep -a "\[RtMintShadow\] .*rows:\|==== \|READY\b" standalone_debug.log | tail -20
echo "=== the five IO rows (+ their TRAP slots) ==="
grep -a -A40 "vtable rows" standalone_debug.log |
  grep -a "FileStream@IO\|VisitorStream@IO\|TRAP slot" | head -60
echo "=== crash log ==="
wc -c standalone_crash.log 2>/dev/null || echo "no crash log"
