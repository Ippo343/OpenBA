#!/usr/bin/env bash
# A/B test: does zero-on-alloc eliminate the intermittent menu heap crash?
# Both arms run with BANISHED_REDZONE=1 (same shim overhead); only ZERO differs.
# Robust outcome detection + reap: the redzone crash handler HANGS the process
# into a zombie that taskkill/SIGTERM cannot reap (only `wmic ... delete` can),
# so we poll for crash.log / Draw and force-reap by name via wmic.
cd "/c/GOG Games/Banished" || exit 9
RUNS=15
MAXW=22   # seconds to wait for an outcome before calling it a hang

reap() { wmic process where "name='reimpl-x64.exe'" delete >/dev/null 2>&1; }

run_one() { # $1 = zero flag ; echoes outcome
  reap; sleep 1
  rm -f standalone_crash.log standalone_debug.log 2>/dev/null
  if [ "$1" = "1" ]; then
    BANISHED_REDZONE=1 BANISHED_REDZONE_ZERO=1 ./reimpl-x64.exe >/dev/null 2>&1 &
  else
    BANISHED_REDZONE=1 ./reimpl-x64.exe >/dev/null 2>&1 &
  fi
  local outcome="HANG"
  for t in $(seq 1 $MAXW); do
    sleep 1
    if [ -f standalone_crash.log ]; then outcome="CRASH"; break; fi
    if grep -q 'MenuDLT::Draw\|MenuScene::Draw\|\[Draw\]' standalone_debug.log 2>/dev/null; then outcome="CLEAN"; break; fi
  done
  reap
  echo "$outcome"
}

run_arm() { # $1 zero ; $2 label
  local c=0 cl=0 h=0
  for i in $(seq 1 $RUNS); do
    o=$(run_one "$1")
    case "$o" in
      CRASH) c=$((c+1));;
      CLEAN) cl=$((cl+1));;
      *)     h=$((h+1));;
    esac
    echo "[$2] run $i: $o"
  done
  echo "=== $2 SUMMARY: crashes=$c cleans=$cl hangs=$h (of $RUNS) ==="
}

echo "###### ARM A: ZERO=0 (baseline) ######"; run_arm 0 "ZERO=0"
echo ""
echo "###### ARM B: ZERO=1 (zero-on-alloc) ######"; run_arm 1 "ZERO=1"
echo "###### DONE ######"
reap
