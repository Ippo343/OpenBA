#!/usr/bin/env bash
# =============================================================================
# tools/cutover_attempt25.sh -- Wave 24 lane CUTOVER, attempt 25
# =============================================================================
# Runs WAVE9_CUTOVER_BOOT_RUNBOOK.md verbatim: rename Runtime-x64.dll aside,
# boot with the mint armed, capture the logs, RESTORE THE DLL UNCONDITIONALLY.
#
# The restore is a trap on EXIT/INT/TERM, and it re-checks presence afterwards.
# Every other lane's differential oracle depends on that file being back.
#
# usage: tools/cutover_attempt25.sh <outdir> [seconds]
set -u
G="/c/GOG Games/Banished"
OUT="${1:?usage: cutover_attempt25.sh <outdir> [seconds]}"
SECS="${2:-90}"
mkdir -p "$OUT"

restore() {
  if [ -f "$G/Runtime-x64.dll.absent" ]; then
    mv -f "$G/Runtime-x64.dll.absent" "$G/Runtime-x64.dll"
    echo "[attempt25] Runtime-x64.dll RESTORED"
  fi
  if [ -f "$G/Runtime-x64.dll" ]; then
    echo "[attempt25] verified present: $(stat -c %s "$G/Runtime-x64.dll") bytes"
  else
    echo "[attempt25] *** FATAL: Runtime-x64.dll IS MISSING ***"
  fi
}
trap restore EXIT INT TERM

echo "[attempt25] exe: $(stat -c '%s bytes  %y' standalone/build/reimpl-x64.exe)"
echo "[attempt25] md5: $(md5sum standalone/build/reimpl-x64.exe | cut -d' ' -f1)"
cp -f standalone/build/reimpl-x64.exe "$G/reimpl-x64.exe"
rm -f "$G/standalone_debug.log" "$G/standalone_error.log" "$G/standalone_crash.log" \
      "$G/standalone_boot_state.txt"

mv -f "$G/Runtime-x64.dll" "$G/Runtime-x64.dll.absent"
ls -l "$G"/Runtime-x64.dll* 2>&1
echo "[attempt25] DLL moved aside; launching"

cd "$G"
BANISHED_RT_NO_DLL=1 BANISHED_RT_MINT=1 BANISHED_RT_MINT_CTOR=1 \
BANISHED_RT_ALLOC=1 BANISHED_RT_STRICT=1 BANISHED_RT_MINT_SHADOW=1 \
BANISHED_AUTOSTART=1 \
./reimpl-x64.exe &
PID=$!
for i in $(seq 1 "$SECS"); do
  kill -0 $PID 2>/dev/null || break
  sleep 1
done
if kill -0 $PID 2>/dev/null; then
  echo "[attempt25] still alive after ${SECS}s -- killing (HANG, treat as inconclusive)"
  kill -9 $PID 2>/dev/null
fi
wait $PID 2>/dev/null
echo "[attempt25] exit status: $?"
cd - >/dev/null

for f in standalone_debug.log standalone_error.log standalone_crash.log standalone_boot_state.txt; do
  [ -f "$G/$f" ] && cp -f "$G/$f" "$OUT/$f"
done
echo "[attempt25] logs copied to $OUT"
