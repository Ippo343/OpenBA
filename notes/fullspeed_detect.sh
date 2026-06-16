#!/usr/bin/env bash
# Full-speed menu crash detector (session 3, 2026-06-15).
#
# Lessons carried in:
#  - GDB and redzone both CHANGE the bug; validate at FULL SPEED (no gdb, no rz).
#  - 0-byte standalone_crash.log = NON-FATAL VEH log, NOT death. Detect crashes
#    by PROCESS DEATH (tasklist alive=0).
#  - stdout (standalone_debug.log) is UNRELIABLE (lost on hard-kill / not flushed)
#    so we CANNOT key on the stdout-printf "MenuDLT::Draw". stderr
#    (standalone_error.log) IS reliable: the render loop prints
#    "[FrameLimiter] fps=" there every ~N frames => proof MainLoop::Run is
#    ticking the Update/Draw cycle (past the menu-scene load).
#  - FS-relative (a682b15, default-on) turns the full-speed CRASH into a 95%-CPU
#    HANG; force OFF with BANISHED_NO_FS_RELATIVE=1 so residual corruption
#    surfaces as a real process-death crash.
#
# Outcome per run:
#   CRASH = process died before MAXW
#   TICK  = alive at MAXW AND stderr shows >=2 FrameLimiter readouts (render loop
#           healthy — strong "reached + sustained MainLoop::Run" signal)
#   STALL = alive at MAXW but no/low frame readouts (loader spin / pre-loop hang)
cd "/c/GOG Games/Banished" || exit 9
RUNS="${RUNS:-20}"
MAXW="${MAXW:-18}"

alive() { tasklist //FI "IMAGENAME eq reimpl-x64.exe" 2>/dev/null | grep -qi reimpl-x64.exe; }
reap()  { wmic process where "name='reimpl-x64.exe'" delete >/dev/null 2>&1; }

run_one() {
  reap; sleep 1
  rm -f standalone_crash.log standalone_debug.log standalone_error.log 2>/dev/null
  BANISHED_NO_FS_RELATIVE=1 ./reimpl-x64.exe >/dev/null 2>&1 &
  local dead=1
  for t in $(seq 1 $MAXW); do
    sleep 1
    if ! alive; then dead=0; break; fi
  done
  local fps; fps=$(grep -c 'FrameLimiter\] fps' standalone_error.log 2>/dev/null || echo 0)
  reap
  if [ "$dead" = "0" ]; then echo "CRASH";
  elif [ "${fps:-0}" -ge 2 ]; then echo "TICK";
  else echo "STALL"; fi
}

c=0; tk=0; st=0
for i in $(seq 1 $RUNS); do
  o=$(run_one)
  case "$o" in
    CRASH) c=$((c+1));;
    TICK)  tk=$((tk+1));;
    *)     st=$((st+1));;
  esac
  printf "run %2d: %s\n" "$i" "$o"
done
echo "=== SUMMARY: crashes=$c ticking=$tk stalls=$st (of $RUNS) ==="
reap
