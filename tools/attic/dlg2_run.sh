#!/usr/bin/env bash
# Wave 10 lane DLG2 -- deploy + run + collect, under ONE game-dir lock.
# Usage: dlg2_run.sh <tag> [extra env assignments...]
set -u
WT="C:/code/GIT/untitled_rev_eng_project/.claude/worktrees/agent-aaa2b6fa4a7217b6e"
GAME="C:/GOG Games/Banished"
TAG="$1"; shift

cd "$WT" || exit 1
echo "=== [DLG2:$TAG] build md5 ==="
md5sum standalone/build/reimpl-x64.exe

"$WT/tools/game_dir_lock.sh" verify lane-DLG2 || { echo "LOCK NOT HELD"; exit 1; }

rm -f "$GAME/standalone_debug.log" "$GAME/standalone_error.log" "$GAME/standalone_crash.log"
cp standalone/build/reimpl-x64.exe "$GAME/reimpl-x64.exe" || exit 1
echo "=== [DLG2:$TAG] deployed md5 ==="
md5sum "$GAME/reimpl-x64.exe"

cd "$GAME" || exit 1
export BANISHED_AUTOSTART=1
export BANISHED_AUTOQUIT=1
export BANISHED_RT_DIFF=1
for kv in "$@"; do export "$kv"; done
./reimpl-x64.exe &
RUNPID=$!
"$WT/tools/boot_state.sh" --wait 300
echo "boot_state exit=$?"
sleep 20
kill "$RUNPID" 2>/dev/null
wait "$RUNPID" 2>/dev/null
echo "=== [DLG2:$TAG] DLG2 lines ==="
grep -aE "DLG2|Dialog\[|\?\?0/\?\?1Dialog" standalone_debug.log standalone_error.log 2>/dev/null
echo "=== [DLG2:$TAG] RtDiff summary ==="
grep -aE "^\[RtDiff\]" standalone_debug.log standalone_error.log 2>/dev/null | grep -aiE "Dialog|TOTAL|summary|sites" | head -40
