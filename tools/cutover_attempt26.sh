#!/usr/bin/env bash
# =============================================================================
# tools/cutover_attempt26.sh -- Wave 25 integration, attempt 26
# =============================================================================
# Runs WAVE9_CUTOVER_BOOT_RUNBOOK.md verbatim: rename Runtime-x64.dll aside,
# boot with the mint armed, capture the logs, RESTORE THE DLL UNCONDITIONALLY.
#
# The restore is a trap on EXIT/INT/TERM, and it re-checks presence afterwards.
# Every other lane's differential oracle depends on that file being back.
#
# =============================================================================
# SUPERSEDED BY tools/cutover_attempt27.sh -- DO NOT COPY THIS FILE'S ENV LINE
# =============================================================================
# Wave 26 lane ATTEND. This script is kept so attempt 26 stays reproducible, and
# its restore trap is still correct. Two things in it are NOT to be reused:
#
#  1. `BANISHED_RT_MINT=1` (below) is the blanket switch. MEASURED: it publishes
#     ALL 202 minted rows BY NAME AND BY ADDRESS, from RegisterMintedData(), 22
#     lines BEFORE standalone/cutover_manifest.cpp runs -- so the manifest's
#     `withheld 1` line describes its INTENT, and the row it names
#     (??_7Dialog@UI@@6B@) was published anyway. Attempt 26's log therefore
#     carries a prediction ("5 by-name doors WILL resolve NULL") that was never
#     in force, and reads as though it had been tested and ruled out.
#
#     Worse, the switch MASKED a real defect: standalone/cutover_manifest.inc
#     was 14 REQUIRED rows short (all of Wave 25 lane DATA's), and the blanket
#     switch published those 14 anyway -- so the boot went deep and
#     `published 44, withheld 1` READ AS CONFIRMATION. The number matched its
#     prediction BECAUSE it was stale. `make` now gates on that file
#     (check-cutover-manifest), so that specific defect cannot recur, but the
#     MASKING PROPERTY of this switch is unchanged.
#
#  2. `BANISHED_RT_MAINLOOPFRAME=0` was set on lane BODY's advice for attempt 26
#     only. MEASURED on one exe with its own control arm: the wall is identical
#     with and without it. It is NOT load-bearing and should not be carried.
#
# Evidence and the run ledger: docs/runtime_port/WAVE26_LANE_ATTEND.md §2-§4.
# Attempt 27 onward takes the env set as an ARGUMENT (so a control arm can be
# re-run on the same exe) and HARD-ABORTS if the DLL is still present after the
# move -- which this script does not, and which cost one run that produced 90s
# of plausible logs from a boot that was not a cutover at all.
#
# This block is here, in the file someone RUNS, and not only in a lane doc, for
# the reason lane PROMOIO put best: a correction that lives only where the
# correcting was done has corrected nothing that anyone will find.
# =============================================================================
#
# usage: tools/cutover_attempt26.sh <outdir> [seconds]
set -u
G="/c/GOG Games/Banished"
OUT="${1:?usage: cutover_attempt26.sh <outdir> [seconds]}"
SECS="${2:-90}"
mkdir -p "$OUT"

restore() {
  if [ -f "$G/Runtime-x64.dll.absent" ]; then
    mv -f "$G/Runtime-x64.dll.absent" "$G/Runtime-x64.dll"
    echo "[attempt26] Runtime-x64.dll RESTORED"
  fi
  if [ -f "$G/Runtime-x64.dll" ]; then
    echo "[attempt26] verified present: $(stat -c %s "$G/Runtime-x64.dll") bytes"
  else
    echo "[attempt26] *** FATAL: Runtime-x64.dll IS MISSING ***"
  fi
}
trap restore EXIT INT TERM

echo "[attempt26] exe: $(stat -c '%s bytes  %y' standalone/build/reimpl-x64.exe)"
echo "[attempt26] md5: $(md5sum standalone/build/reimpl-x64.exe | cut -d' ' -f1)"
cp -f standalone/build/reimpl-x64.exe "$G/reimpl-x64.exe"
rm -f "$G/standalone_debug.log" "$G/standalone_error.log" "$G/standalone_crash.log" \
      "$G/standalone_boot_state.txt"

mv -f "$G/Runtime-x64.dll" "$G/Runtime-x64.dll.absent"
ls -l "$G"/Runtime-x64.dll* 2>&1
echo "[attempt26] DLL moved aside; launching"

# Said at RUN time, not only in the header: a comment reaches a reader, and the
# person about to misread this run is the one who typed the command.
echo "[attempt26] NOTE: superseded by tools/cutover_attempt27.sh."
echo "[attempt26]   BANISHED_RT_MINT=1 below publishes ALL 202 minted rows before"
echo "[attempt26]   the manifest runs, so any '[CutoverManifest] withheld N' line"
echo "[attempt26]   in this run's log states INTENT, not the registration state."
echo "[attempt26]   BANISHED_RT_MAINLOOPFRAME=0 is measured NOT load-bearing."
echo "[attempt26]   See docs/runtime_port/WAVE26_LANE_ATTEND.md 2-4."

cd "$G"
BANISHED_RT_NO_DLL=1 BANISHED_RT_MINT=1 BANISHED_RT_MINT_CTOR=1 \
BANISHED_RT_ALLOC=1 BANISHED_RT_STRICT=1 BANISHED_RT_MINT_SHADOW=1 \
BANISHED_AUTOSTART=1 BANISHED_RT_MAINLOOPFRAME=0 \
./reimpl-x64.exe &
PID=$!
for i in $(seq 1 "$SECS"); do
  kill -0 $PID 2>/dev/null || break
  sleep 1
done
if kill -0 $PID 2>/dev/null; then
  echo "[attempt26] still alive after ${SECS}s -- killing (HANG, treat as inconclusive)"
  kill -9 $PID 2>/dev/null
fi
wait $PID 2>/dev/null
echo "[attempt26] exit status: $?"
cd - >/dev/null

for f in standalone_debug.log standalone_error.log standalone_crash.log standalone_boot_state.txt; do
  [ -f "$G/$f" ] && cp -f "$G/$f" "$OUT/$f"
done
echo "[attempt26] logs copied to $OUT"
