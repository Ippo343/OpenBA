#!/usr/bin/env bash
# Session 5: confirm the External::Load serialize lock doesn't break/stall the
# in-game world build. BANISHED_AUTOSTART drives menu -> New Game -> Start ->
# in-game. Success marker = "[GUL::Load] EXIT — world built" in stderr.
# Outcome: BUILT (world built, no crash), CRASH (process death), or NOBUILD
# (alive but never logged world-built within MAXW = stall/incomplete).
cd "/c/GOG Games/Banished" || exit 9
RUNS="${RUNS:-6}"; MAXW="${MAXW:-40}"
alive() { tasklist //FI "IMAGENAME eq reimpl-x64.exe" 2>/dev/null | grep -qi reimpl-x64.exe; }
reap()  { wmic process where "name='reimpl-x64.exe'" delete >/dev/null 2>&1; }
b=0; c=0; nb=0
for i in $(seq 1 $RUNS); do
  reap; sleep 1; rm -f standalone_error.log
  BANISHED_AUTOSTART=1 ./reimpl-x64.exe >/dev/null 2>&1 &
  built=0; dead=1
  for t in $(seq 1 $MAXW); do
    sleep 1
    if grep -q 'EXIT — world built' standalone_error.log 2>/dev/null; then built=1; fi
    if ! alive; then dead=0; break; fi
    [ "$built" = 1 ] && break
  done
  reap
  if [ "$built" = 1 ]; then b=$((b+1)); o=BUILT;
  elif [ "$dead" = 0 ]; then c=$((c+1)); o=CRASH;
  else nb=$((nb+1)); o=NOBUILD; fi
  printf "run %2d: %s\n" "$i" "$o"
done
echo "=== AUTOSTART SUMMARY: built=$b crash=$c nobuild=$nb (of $RUNS) ==="
reap
