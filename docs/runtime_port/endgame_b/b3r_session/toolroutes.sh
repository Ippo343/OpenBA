#!/usr/bin/env bash
# tool harness routes: placement / road / removal (ToolUI child dialog, drag tools)
set -u
cd /c/code/GIT/untitled_rev_eng_project
G="/c/GOG Games/Banished"; K="$(cd "$(dirname "$0")" && pwd)"; OUT="$K/runs/${1:-tools}"; mkdir -p "$OUT"
tools/game_dir_lock.sh acquire coord 1800 >/dev/null || { echo lock failed; exit 1; }
trap 'tools/game_dir_lock.sh release coord >/dev/null' EXIT
for t in placement road removal; do
  cp -f standalone/build/reimpl-x64.exe "$G/reimpl-x64.exe"
  rm -f "$G"/standalone_{debug,error,crash,hang}.log "$G/standalone_boot_state.txt"
  ( cd "$G" && BANISHED_AUTOSTART=1 BANISHED_TOOL_HARNESS=$t BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=120 timeout 400 ./reimpl-x64.exe ); rc=$?
  mkdir -p "$OUT/$t"; for f in standalone_debug.log standalone_error.log standalone_crash.log standalone_hang.log; do [ -f "$G/$f" ] && cp -f "$G/$f" "$OUT/$t/"; done
  echo "tool $t: exit $rc crash=$(stat -c %s "$G/standalone_crash.log" 2>/dev/null || echo 0) hang=$([ -f "$G/standalone_hang.log" ] && echo YES || echo no) | $(cat "$OUT/$t"/standalone_*.log | grep -E '\[ToolHarness\]' | tail -2 | tr '\n' ' ' | cut -c1-300)"
done
