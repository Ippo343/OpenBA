#!/usr/bin/env bash
# Capture full-speed residual crash dumps (session 3).
# NO redzone (true full-speed timing), NO_FS_RELATIVE=1 (crash not hang).
# On process death, the CrashHandler (SetUnhandledExceptionFilter) has written a
# full dump to standalone_crash.log (registers + module+off + fault + heap walk +
# stack). Preserve it. Stop after CATCH crashes or RUNS attempts.
cd "/c/GOG Games/Banished" || exit 9
RUNS="${RUNS:-25}"
MAXW="${MAXW:-18}"
CATCH="${CATCH:-3}"
OUT=/c/code/GIT/untitled_rev_eng_project/notes/wp_runs
mkdir -p "$OUT"

alive() { tasklist //FI "IMAGENAME eq reimpl-x64.exe" 2>/dev/null | grep -qi reimpl-x64.exe; }
reap()  { wmic process where "name='reimpl-x64.exe'" delete >/dev/null 2>&1; }

caught=0
for i in $(seq 1 $RUNS); do
  reap; sleep 1
  rm -f standalone_crash.log standalone_debug.log standalone_error.log 2>/dev/null
  BANISHED_NO_FS_RELATIVE=1 ./reimpl-x64.exe >/dev/null 2>&1 &
  dead=1
  for t in $(seq 1 $MAXW); do
    sleep 1
    if ! alive; then dead=0; break; fi
  done
  if [ "$dead" = "0" ]; then
    caught=$((caught+1))
    cp standalone_crash.log "$OUT/fs_crash_${i}.txt" 2>/dev/null
    cp standalone_error.log "$OUT/fs_err_${i}.txt" 2>/dev/null
    sz=$(stat -c%s "$OUT/fs_crash_${i}.txt" 2>/dev/null || echo 0)
    echo "run $i: CRASH (caught #$caught, crashlog=${sz}B -> fs_crash_${i}.txt)"
    if [ "$caught" -ge "$CATCH" ]; then reap; echo "got $CATCH crashes, stopping"; break; fi
  else
    echo "run $i: alive"
  fi
  reap
done
echo "=== captured $caught crash dumps ==="
