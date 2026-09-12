#!/usr/bin/env bash
# =============================================================================
# tools/cutover_run.sh -- the cutover boot gate (was cutover_attempt32.sh)
# =============================================================================
# Mechanically identical to tools/cutover_attempt30.sh (same DLL move-aside,
# same hard gate, same unconditional restore trap, same md5-on-both-sides of
# the copy). ONE THING IS ADDED AND IT IS THE POINT OF THIS FILE:
#
#   *** THE ARM IS WRITTEN DOWN HERE, DERIVED, RATHER THAN RECONSTRUCTED. ***
#
# WAVE28_CLOSED §2.1 records three wasted runs caused by guessing the arm, and
# WAVE29_KICKOFF §0.4 says to recover it from the `[RtAlloc] ATTEST:` line and
# never from a log grep for `BANISHED_*` strings (that matches printed prose,
# not the environment). Lane WALL did exactly that against the SURVIVING
# attempt-31 logs -- and §0.4's own summary of the arm turned out to be
# INCOMPLETE BY TWO VARIABLES. See docs/runtime_port/WAVE29_LANE_WALL.md §3.
#
# THE FOUR VARIABLES ATTEMPT 31 ACTUALLY RAN WITH, each with the line that
# proves it, each printed BY THE CODE THAT READ THE VARIABLE:
#
#   BANISHED_RT_NO_DLL=1
#       standalone_error.log:1
#       "[Standalone] BANISHED_RT_NO_DLL=1 -- continuing with Runtime-x64.dll
#        ABSENT."
#   BANISHED_RT_ALLOC=1
#       standalone_error.log:6
#       "[RtAlloc] ATTEST: ARMED (cutover, DLL absent) (exports=27)"
#       REQUIRED at cutover, HARMFUL with the DLL present -- route-scoped.
#   BANISHED_RT_MINT=1                              <-- MISSING FROM §0.4
#       standalone_debug.log:137
#       "[RtMint] ... registration is ON (BANISHED_RT_MINT=1)"
#       That string is emitted from a ternary on EnvOn("BANISHED_RT_MINT")
#       (standalone/rt_data_mint.cpp:1093-1097), so it cannot print without
#       the variable. NOTE that cutover_attempt30.sh's own header calls this
#       "the blanket switch integrator-queue item C3 says not to use at
#       cutover". That guidance is STALE: attempt 31 ran with it, and without
#       it the minted `?_typeInfo@...` data rows the ported bodies read are
#       not registered at all.
#   BANISHED_AUTOSTART=1                            <-- MISSING FROM §0.4
#       standalone_error.log:36
#       "[AutoStart] ENABLED (BANISHED_AUTOSTART=\"1\")"
#       Also what makes HarnessGuardUnattendedBoot clear the launcher-dialog
#       registry bit, so dropping it risks a BLOCKED_DIALOG boot that looks
#       like a slow load.
#
# AND ONE THAT MUST STAY UNSET:
#   BANISHED_RT_DIFF     `grep -c "RtDiff] ON"` on attempt 31's logs = 0.
#       RtDiff runs the original alongside the port; with the DLL absent the
#       original is null and the run dies at t~6ms. This is failure #2 of
#       WAVE28_CLOSED §2.1.
#
# usage: tools/cutover_attempt32.sh <outdir> <seconds> [<exe-to-deploy>]
#        (the arm is NOT an argument -- it is the four lines below. To run a
#         DIFFERENT arm, use cutover_attempt30.sh, which takes one.)
set -u
G="/c/GOG Games/Banished"
OUT="${1:?usage: cutover_run.sh <outdir> <seconds> [exe]}"
SECS="${2:?usage: cutover_run.sh <outdir> <seconds> [exe]}"
EXE="${3:-standalone/build/reimpl-x64.exe}"
mkdir -p "$OUT"

ARM=(BANISHED_RT_NO_DLL=1
     BANISHED_RT_ALLOC=1
     BANISHED_RT_MINT=1
     BANISHED_AUTOSTART=1)

restore() {
  if [ -f "$G/Runtime-x64.dll.absent" ]; then
    mv -f "$G/Runtime-x64.dll.absent" "$G/Runtime-x64.dll"
    echo "[cutover] Runtime-x64.dll RESTORED"
  fi
  if [ -f "$G/Runtime-x64.dll" ]; then
    echo "[cutover] verified present: $(stat -c %s "$G/Runtime-x64.dll") bytes"
  else
    echo "[cutover] *** FATAL: Runtime-x64.dll IS MISSING ***"
  fi
}
trap restore EXIT INT TERM

echo "[cutover] source exe : $EXE"
echo "[cutover] build md5  : $(md5sum "$EXE" | cut -d' ' -f1)"
cp -f "$EXE" "$G/reimpl-x64.exe"
echo "[cutover] deployed md5: $(md5sum "$G/reimpl-x64.exe" | cut -d' ' -f1)"
echo "[cutover] arm: ${ARM[*]}   (BANISHED_RT_DIFF deliberately UNSET)"
rm -f "$G/standalone_debug.log" "$G/standalone_error.log" "$G/standalone_crash.log" \
      "$G/standalone_boot_state.txt"

mv -f "$G/Runtime-x64.dll" "$G/Runtime-x64.dll.absent"
# HARD GATE, inherited verbatim from attempt30.sh. A cutover attempt whose
# defining condition silently did not hold is worse than no run.
if [ -f "$G/Runtime-x64.dll" ]; then
  echo "[cutover] *** ABORT: Runtime-x64.dll is STILL PRESENT after the move."
  echo "[cutover] *** This would NOT have been a cutover run. Nothing launched."
  exit 2
fi
echo "[cutover] DLL moved aside (verified absent); launching"

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
# ATTEST-LINE PROOF, copied into the outdir so the NEXT wave does not have to
# find the live logs before a sibling lane overwrites them. Wave 28's evidence
# directories contain only `standalone_boot_state.txt` because `*.log` is in
# .gitignore -- see WAVE29_LANE_WALL.md §5, defect 2.
{
  echo "== arm, as the RUNNING CODE reported it =="
  grep -hE "BANISHED_RT_NO_DLL=1 --|RtAlloc\] ATTEST:|registration is (ON|off)|AutoStart\] ENABLED|RtDiff\] ON" \
       "$OUT/standalone_error.log" "$OUT/standalone_debug.log" 2>/dev/null
  echo "== RtDiff ON count (MUST be 0) =="
  grep -hc "RtDiff] ON" "$OUT/standalone_error.log" "$OUT/standalone_debug.log" 2>/dev/null
} > "$OUT/attest.txt"
echo "[cutover] logs + attest.txt copied to $OUT"
