#!/usr/bin/env bash
# =============================================================================
# tools/cutover_run.sh -- the boot gate ("cutover" is a legacy name)
# =============================================================================
# Since ENDGAME A.1 (2026-09-13) the exe never loads Runtime-x64.dll -- there
# is no DLL to move aside any more. This script deploys the debug exe, boots
# it bounded by <seconds>, and copies the logs plus an attest.txt to <outdir>.
# For the old DLL-present-vs-absent procedure, check out the `dll-present-last`
# tag (commit 86d28a73), the last commit where the DLL was still loaded.
#
# usage: tools/cutover_run.sh <outdir> <seconds> [<exe-to-deploy>]
set -u
G="/c/GOG Games/Banished"
OUT="${1:?usage: cutover_run.sh <outdir> <seconds> [exe]}"
SECS="${2:?usage: cutover_run.sh <outdir> <seconds> [exe]}"
EXE="${3:-standalone/build/reimpl-x64.exe}"
mkdir -p "$OUT"

ARM=(BANISHED_RT_CENSUS=1
     BANISHED_AUTOSTART=1)

echo "[cutover] source exe : $EXE"
echo "[cutover] build md5  : $(md5sum "$EXE" | cut -d' ' -f1)"
cp -f "$EXE" "$G/reimpl-x64.exe"
echo "[cutover] deployed md5: $(md5sum "$G/reimpl-x64.exe" | cut -d' ' -f1)"
echo "[cutover] arm: ${ARM[*]}   (BANISHED_RT_DIFF deliberately UNSET)"
rm -f "$G/standalone_debug.log" "$G/standalone_error.log" "$G/standalone_crash.log" \
      "$G/standalone_boot_state.txt"

cd "$G"
env "${ARM[@]}" ./reimpl-x64.exe &
PID=$!
for i in $(seq 1 "$SECS"); do
  kill -0 $PID 2>/dev/null || break
  sleep 1
done
if kill -0 $PID 2>/dev/null; then
  echo "[cutover] still alive after ${SECS}s -- killing (HANG, treat as inconclusive)"
  kill -9 $PID 2>/dev/null
fi
wait $PID 2>/dev/null
echo "[cutover] exit status: $?"
cd - >/dev/null

for f in standalone_debug.log standalone_error.log standalone_crash.log standalone_boot_state.txt; do
  [ -f "$G/$f" ] && cp -f "$G/$f" "$OUT/$f"
done
# ATTEST-LINE PROOF, copied into the outdir so the next run does not have to
# find the live logs before another run overwrites them.
{
  echo "== arm, as the RUNNING CODE reported it =="
  grep -hE "never loaded \(ENDGAME A.1\)|registration is (ON|off)|AutoStart\] ENABLED|RtDiff\] ON" \
       "$OUT/standalone_error.log" "$OUT/standalone_debug.log" 2>/dev/null
  echo "== RtDiff ON count (MUST be 0) =="
  grep -hc "RtDiff] ON" "$OUT/standalone_error.log" "$OUT/standalone_debug.log" 2>/dev/null
} > "$OUT/attest.txt"
echo "[cutover] logs + attest.txt copied to $OUT"
