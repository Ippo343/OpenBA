#!/usr/bin/env bash
# =============================================================================
# tools/cutover_attempt30.sh -- Wave 26 lane ATTEND, cutover attempt 30
# =============================================================================
# Derived from tools/cutover_attempt26.sh.  TWO THINGS CHANGED, BOTH DELIBERATE:
#
#  1. THE ENV SET IS AN ARGUMENT, not baked in.  Attempt 26 hard-coded
#     `BANISHED_RT_MINT=1` (the blanket switch integrator-queue item C3 says not
#     to use at cutover) and `BANISHED_RT_MAINLOOPFRAME=0`.  Attempt 27 must run
#     WITHOUT both, and must also be able to re-run attempt 26's exact line as a
#     CONTROL on the same tip and the same exe -- otherwise a moved wall cannot
#     be told from a moved tree.  So the caller passes the arm.
#
#  2. It records the exe md5 on BOTH sides of the copy, because the game-dir
#     lock's STALE_AFTER is shorter than a legitimate measurement and the holder
#     is never told when its lock is broken (WAVE26_KICKOFF standing warning 1).
#
# KEPT EXACTLY: the unconditional EXIT/INT/TERM trap that renames
# Runtime-x64.dll back and re-verifies its byte count.  Every other lane's
# differential oracle depends on that file being back.
#
# usage: tools/cutover_attempt30.sh <outdir> <seconds> VAR=VAL [VAR=VAL ...]
set -u
G="/c/GOG Games/Banished"
OUT="${1:?usage: cutover_attempt30.sh <outdir> <seconds> VAR=VAL ...}"
SECS="${2:?usage: cutover_attempt30.sh <outdir> <seconds> VAR=VAL ...}"
shift 2
mkdir -p "$OUT"

restore() {
  if [ -f "$G/Runtime-x64.dll.absent" ]; then
    mv -f "$G/Runtime-x64.dll.absent" "$G/Runtime-x64.dll"
    echo "[attempt30] Runtime-x64.dll RESTORED"
  fi
  if [ -f "$G/Runtime-x64.dll" ]; then
    echo "[attempt30] verified present: $(stat -c %s "$G/Runtime-x64.dll") bytes"
  else
    echo "[attempt30] *** FATAL: Runtime-x64.dll IS MISSING ***"
  fi
}
trap restore EXIT INT TERM

echo "[attempt30] build md5 : $(md5sum standalone/build/reimpl-x64.exe | cut -d' ' -f1)"
cp -f standalone/build/reimpl-x64.exe "$G/reimpl-x64.exe"
echo "[attempt30] deployed md5: $(md5sum "$G/reimpl-x64.exe" | cut -d' ' -f1)"
echo "[attempt30] env: $*"
rm -f "$G/standalone_debug.log" "$G/standalone_error.log" "$G/standalone_crash.log" \
      "$G/standalone_boot_state.txt"

mv -f "$G/Runtime-x64.dll" "$G/Runtime-x64.dll.absent"
# HARD GATE, added after it bit this lane once. `mv` failing with "Device or
# resource busy" (a stale handle on the DLL) does NOT stop the script -- the
# game then launches with the DLL STILL PRESENT and the run looks like an
# ordinary boot that mysteriously did not reach the cutover wall. A cutover
# attempt whose defining condition silently did not hold is worse than no run.
if [ -f "$G/Runtime-x64.dll" ]; then
  echo "[attempt30] *** ABORT: Runtime-x64.dll is STILL PRESENT after the move."
  echo "[attempt30] *** This would NOT have been a cutover run. Nothing launched."
  echo "[attempt30] *** Check for a lingering reimpl-x64.exe (tools/game_dir_lock.sh orphans)."
  exit 2
fi
echo "[attempt30] DLL moved aside (verified absent); launching"

cd "$G"
env "$@" ./reimpl-x64.exe &
PID=$!
for i in $(seq 1 "$SECS"); do
  kill -0 $PID 2>/dev/null || break
  sleep 1
done
if kill -0 $PID 2>/dev/null; then
  echo "[attempt30] still alive after ${SECS}s -- killing (HANG, treat as inconclusive)"
  kill -9 $PID 2>/dev/null
fi
wait $PID 2>/dev/null
echo "[attempt30] exit status: $?"
cd - >/dev/null

for f in standalone_debug.log standalone_error.log standalone_crash.log standalone_boot_state.txt; do
  [ -f "$G/$f" ] && cp -f "$G/$f" "$OUT/$f"
done
echo "[attempt30] logs copied to $OUT"
