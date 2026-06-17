#!/usr/bin/env bash
# Session 5: validate the DEFAULT build (FindOrLoad lock on by default) — NO
# BANISHED_* env at all, exactly as a user launches it. Menu mode (no autostart).
# TICK = reached + sustained the render loop (menu loaded). CRASH = process death.
cd "/c/GOG Games/Banished" || exit 9
RUNS="${RUNS:-20}"
MAXW="${MAXW:-16}"
alive() { tasklist //FI "IMAGENAME eq reimpl-x64.exe" 2>/dev/null | grep -qi reimpl-x64.exe; }
reap()  { wmic process where "name='reimpl-x64.exe'" delete >/dev/null 2>&1; }
c=0; tk=0; st=0
for i in $(seq 1 $RUNS); do
  reap; sleep 1
  rm -f standalone_error.log 2>/dev/null
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
echo "=== DEFAULT SUMMARY: crashes=$c ticking=$tk stalls=$st (of $RUNS) ==="
reap
