#!/usr/bin/env bash
# Session 4: full-speed capture WITH the Runtime ~Filename/operator= detour
# (BANISHED_FNHOOK=1). On a crash, preserve standalone_error.log (the [FNHook]
# UAF report + the geometry stack land there) and standalone_crash.log.
# stderr is the reliable channel; FNHook flushes each report before the later
# fatal Allocate, so the report survives the hard crash.
cd "/c/GOG Games/Banished" || exit 9
RUNS="${RUNS:-30}"
MAXW="${MAXW:-18}"
CATCH="${CATCH:-3}"
OUT=/c/code/GIT/untitled_rev_eng_project/notes/wp_runs
mkdir -p "$OUT"

alive() { tasklist //FI "IMAGENAME eq reimpl-x64.exe" 2>/dev/null | grep -qi reimpl-x64.exe; }
reap()  { wmic process where "name='reimpl-x64.exe'" delete >/dev/null 2>&1; }

caught=0; hooks=0
for i in $(seq 1 $RUNS); do
  reap; sleep 1
  rm -f standalone_crash.log standalone_debug.log standalone_error.log 2>/dev/null
  BANISHED_NO_FS_RELATIVE=1 BANISHED_FNHOOK=1 ./reimpl-x64.exe >/dev/null 2>&1 &
  dead=1
  for t in $(seq 1 $MAXW); do
    sleep 1
    if ! alive; then dead=0; break; fi
  done
  fh=$(grep -c 'FNHook] \*\*\* UAF' standalone_error.log 2>/dev/null); fh=${fh:-0}
  if [ "$dead" = "0" ]; then
    caught=$((caught+1))
    cp standalone_error.log "$OUT/fnhook_err_${i}.txt" 2>/dev/null
    cp standalone_crash.log "$OUT/fnhook_crash_${i}.txt" 2>/dev/null
    echo "run $i: CRASH (caught #$caught, FNHook UAF reports=$fh -> fnhook_err_${i}.txt)"
    [ "${fh:-0}" -ge 1 ] && hooks=$((hooks+1))
    if [ "$caught" -ge "$CATCH" ]; then reap; echo "got $CATCH crashes, stopping"; break; fi
  else
    echo "run $i: alive (FNHook UAF reports=$fh)"
    # Even on a non-crashing run, a UAF report is gold — save it.
    if [ "${fh:-0}" -ge 1 ]; then
      cp standalone_error.log "$OUT/fnhook_err_alive_${i}.txt" 2>/dev/null
      hooks=$((hooks+1))
      echo "   -> saved UAF report from a NON-crashing run (fnhook_err_alive_${i}.txt)"
    fi
  fi
  reap
done
echo "=== captured $caught crashes; $hooks run(s) had a [FNHook] UAF report ==="
reap
