#!/usr/bin/env bash
# Wave 10 lane DLG2 -- the whole verification matrix under ONE lock hold.
set -u
WT="C:/code/GIT/untitled_rev_eng_project/.claude/worktrees/agent-aaa2b6fa4a7217b6e"
GAME="C:/GOG Games/Banished"

cd "$WT" || exit 1
echo "=== build md5 ==="; md5sum standalone/build/reimpl-x64.exe
"$WT/tools/game_dir_lock.sh" verify lane-DLG2 || { echo "LOCK NOT HELD"; exit 1; }
cp standalone/build/reimpl-x64.exe "$GAME/reimpl-x64.exe" || exit 1
echo "=== deployed md5 ==="; md5sum "$GAME/reimpl-x64.exe"

run_one() {   # $1=tag  $2=seconds  rest: KEY=VAL
  local tag="$1"; shift
  local secs="$1"; shift
  cd "$GAME" || return 1
  rm -f standalone_debug.log standalone_error.log standalone_crash.log
  ( export BANISHED_RT_DIFF=1
    for kv in "$@"; do export "$kv"; done
    ./reimpl-x64.exe & echo $! > /tmp/dlg2.pid
    wait $(cat /tmp/dlg2.pid) 2>/dev/null ) &
  local shpid=$!
  "$WT/tools/boot_state.sh" --wait 300 >/dev/null 2>&1
  sleep "$secs"
  local p; p=$(cat /tmp/dlg2.pid 2>/dev/null)
  [ -n "$p" ] && kill "$p" 2>/dev/null
  sleep 2
  echo "---------------- [$tag] ----------------"
  grep -aE "DLG2 diff|DLG2 pin control|Dialog\[pins\]|FIRST LIVE CALL" standalone_debug.log 2>/dev/null
  grep -ac "FIRST LIVE CALL" standalone_debug.log 2>/dev/null | sed 's/^/  live-call lines: /'
}

run_one negctrl1 25 BANISHED_AUTOSTART=1 BANISHED_DLG2_NEGCTRL=1
run_one negctrl2 25 BANISHED_AUTOSTART=1 BANISHED_DLG2_NEGCTRL=2
run_one negctrl3 25 BANISHED_AUTOSTART=1 BANISHED_DLG2_NEGCTRL=3
run_one negctrl4 25 BANISHED_AUTOSTART=1 BANISHED_DLG2_NEGCTRL=4
run_one alloc_armed 25 BANISHED_AUTOSTART=1 BANISHED_RT_ALLOC=1
run_one live_menu 60 BANISHED_AUTOMENU_BUTTON=7 BANISHED_AUTOMENU_CLICKS=1001,5001,d0,4001,101,m6 BANISHED_MODAL_ANSWER=1
run_one live_game 90 BANISHED_AUTOSTART=1
