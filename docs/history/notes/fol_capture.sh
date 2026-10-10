#!/usr/bin/env bash
# Session 5 (2026-06-16): capture which resource FindOrLoad returns NULL for
# during the menu load — the dominant residual crash (texture-null / dialog-null,
# heap intact). PIN on (removes the data-dir link face so only bug B remains),
# NO_FS_RELATIVE on, BANISHED_FOL_NULLWATCH on (logs every FindOrLoad->NULL with
# the resource path to stderr / standalone_error.log).
# Preserve the error log on BOTH crash and alive runs (the null paths appear in
# both; a crash run shows the null that got dereferenced).
cd "/c/GOG Games/Banished" || exit 9
RUNS="${RUNS:-20}"
MAXW="${MAXW:-16}"
CATCH="${CATCH:-3}"
OUT=/c/code/GIT/untitled_rev_eng_project/notes/wp_runs
mkdir -p "$OUT"

alive() { tasklist //FI "IMAGENAME eq reimpl-x64.exe" 2>/dev/null | grep -qi reimpl-x64.exe; }
reap()  { wmic process where "name='reimpl-x64.exe'" delete >/dev/null 2>&1; }

caught=0
for i in $(seq 1 $RUNS); do
  reap; sleep 1
  rm -f standalone_crash.log standalone_debug.log standalone_error.log 2>/dev/null
  BANISHED_NO_FS_RELATIVE=1 BANISHED_PIN_DATADIR=1 BANISHED_FOL_NULLWATCH=1 \
    ./reimpl-x64.exe >/dev/null 2>&1 &
  dead=1
  for t in $(seq 1 $MAXW); do
    sleep 1
    if ! alive; then dead=0; break; fi
  done
  nulls=$(grep -c 'FOLNull' standalone_error.log 2>/dev/null || echo 0)
  if [ "$dead" = "0" ]; then
    caught=$((caught+1))
    cp standalone_error.log "$OUT/fol_crash_${i}.err" 2>/dev/null
    cp standalone_crash.log "$OUT/fol_crash_${i}.txt" 2>/dev/null
    echo "run $i: CRASH (#$caught) FOLNull-lines=$nulls -> fol_crash_${i}.err"
    if [ "$caught" -ge "$CATCH" ]; then reap; echo "got $CATCH crashes"; break; fi
  else
    cp standalone_error.log "$OUT/fol_alive_${i}.err" 2>/dev/null
    echo "run $i: alive FOLNull-lines=$nulls"
  fi
  reap
done
echo "=== done (crashes=$caught) ==="
