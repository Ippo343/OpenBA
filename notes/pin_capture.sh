#!/usr/bin/env bash
# Session 5 (2026-06-16): capture residual crash dumps WITH the data-dir pin on.
# Same as fullspeed_capture.sh but BANISHED_PIN_DATADIR=1 (+ NO_FS_RELATIVE=1).
# Goal: classify the residual crash FACE under pin —
#   Runtime+0xC74C3 / +0xC74EA  = free-list link (data-dir/path-String UAF) face
#   Runtime+0x102C9F / +0x103933 = MaterialInstance null-texture face
# If pin removes the link face and only texture-null remains, the data-dir pin
# fixed the link face and texture-null is a separate (factory/resource) bug.
cd "/c/GOG Games/Banished" || exit 9
RUNS="${RUNS:-30}"
MAXW="${MAXW:-18}"
CATCH="${CATCH:-4}"
TAG="${TAG:-pin}"
OUT=/c/code/GIT/untitled_rev_eng_project/notes/wp_runs
mkdir -p "$OUT"

alive() { tasklist //FI "IMAGENAME eq reimpl-x64.exe" 2>/dev/null | grep -qi reimpl-x64.exe; }
reap()  { wmic process where "name='reimpl-x64.exe'" delete >/dev/null 2>&1; }

caught=0
for i in $(seq 1 $RUNS); do
  reap; sleep 1
  rm -f standalone_crash.log standalone_debug.log standalone_error.log 2>/dev/null
  BANISHED_NO_FS_RELATIVE=1 BANISHED_PIN_DATADIR=1 ./reimpl-x64.exe >/dev/null 2>&1 &
  dead=1
  for t in $(seq 1 $MAXW); do
    sleep 1
    if ! alive; then dead=0; break; fi
  done
  if [ "$dead" = "0" ]; then
    caught=$((caught+1))
    cp standalone_crash.log "$OUT/${TAG}_crash_${i}.txt" 2>/dev/null
    face=$(grep -m1 -iE "Runtime-x64.dll \+ 0x" "$OUT/${TAG}_crash_${i}.txt" 2>/dev/null | sed 's/^[[:space:]]*//')
    echo "run $i: CRASH (#$caught) face=[$face]"
    if [ "$caught" -ge "$CATCH" ]; then reap; echo "got $CATCH crashes, stopping"; break; fi
  else
    echo "run $i: alive"
  fi
  reap
done
echo "=== captured $caught crash dumps (tag=$TAG) ==="
