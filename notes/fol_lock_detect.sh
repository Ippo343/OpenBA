#!/usr/bin/env bash
# Session 5 (2026-06-16): A/B test the FindOrLoad serialize lock for bug B
# (intermittent resource-load NULLs from a 2-thread race in External::Load).
# Crash = process death. PIN on (removes bug A link face). Set LOCK=1 to enable
# BANISHED_FOL_LOCK (serialize FindOrLoad), LOCK=0 for the control.
cd "/c/GOG Games/Banished" || exit 9
RUNS="${RUNS:-20}"
MAXW="${MAXW:-16}"
LOCK="${LOCK:-1}"
PIN="${PIN:-1}"

alive() { tasklist //FI "IMAGENAME eq reimpl-x64.exe" 2>/dev/null | grep -qi reimpl-x64.exe; }
reap()  { wmic process where "name='reimpl-x64.exe'" delete >/dev/null 2>&1; }

c=0; tk=0; st=0
for i in $(seq 1 $RUNS); do
  reap; sleep 1
  rm -f standalone_error.log 2>/dev/null
  BANISHED_NO_FS_RELATIVE=1 BANISHED_PIN_DATADIR=$PIN BANISHED_FOL_LOCK=$LOCK \
    ./reimpl-x64.exe >/dev/null 2>&1 &
  dead=1
  for t in $(seq 1 $MAXW); do sleep 1; if ! alive; then dead=0; break; fi; done
  fps=$(grep -c 'FrameLimiter\] fps' standalone_error.log 2>/dev/null || echo 0)
  reap
  if [ "$dead" = "0" ]; then c=$((c+1)); o=CRASH;
  elif [ "${fps:-0}" -ge 2 ]; then tk=$((tk+1)); o=TICK;
  else st=$((st+1)); o=STALL; fi
  printf "run %2d: %s\n" "$i" "$o"
done
echo "=== LOCK=$LOCK SUMMARY: crashes=$c ticking=$tk stalls=$st (of $RUNS) ==="
reap
