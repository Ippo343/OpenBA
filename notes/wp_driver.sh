#!/usr/bin/env bash
# Relaunch the standalone exe under gdb until the bucket-12-head watchpoint
# fires (the bit-32 wild write). Each run: clean menu => timeout (relaunch);
# Face-B SIGSEGV in MaterialInstance::Setup => relaunch; watchpoint catch =>
# "Old value =" appears => save and stop.
GDB="C:/msys64/ucrt64/bin/gdb.exe"
EXE="C:/GOG Games/Banished/reimpl-x64.exe"
SCRIPT="C:/code/GIT/untitled_rev_eng_project/notes/catch_wp.gdb"
OUTDIR="C:/code/GIT/untitled_rev_eng_project/notes/wp_runs"
mkdir -p "$OUTDIR"
MAX=40
reap() { wmic process where "name='reimpl-x64.exe'" delete >/dev/null 2>&1; wmic process where "name='gdb.exe'" delete >/dev/null 2>&1; }
for i in $(seq 1 $MAX); do
  # reap any zombie from a previous run (redzone crashes hang unkillable by taskkill)
  reap; sleep 1
  OUT="$OUTDIR/run_$i.txt"
  echo "[driver] === run $i ==="
  timeout 35 "$GDB" --batch -x "$SCRIPT" "$EXE" > "$OUT" 2>&1
  reap
  if grep -q "Old value =" "$OUT"; then
    echo "[driver] *** WATCHPOINT CAUGHT on run $i -> $OUT ***"
    echo "[driver] head/registers/backtrace follow:"
    sed -n '/=== STOPPED ===/,/=== DONE ===/p' "$OUT"
    exit 0
  elif grep -q "Setup@MaterialInstance" "$OUT"; then
    echo "[driver] run $i: Face-B SIGSEGV (MaterialInstance::Setup) -> relaunch"
  elif grep -q "=== STOPPED ===" "$OUT"; then
    echo "[driver] run $i: stopped (non-watchpoint signal) -> relaunch"
    grep -m1 "SIGSEGV\|signal" "$OUT" || true
  else
    echo "[driver] run $i: clean menu (timeout) -> relaunch"
  fi
done
echo "[driver] exhausted $MAX runs without a watchpoint catch"
exit 1
