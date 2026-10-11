#!/usr/bin/env bash
# One forced-fire new-game route (the disaster harness): the FireComponent / DestroyComponent path
# COMP-B unguarded as retail (B8 C3). usage: disaster_route.sh <label>   EDIT WITH THE Write TOOL ONLY.
set -u
REPO=/c/code/GIT/untitled_rev_eng_project
G="/c/GOG Games/Banished"
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$HERE/runs/$1"; mkdir -p "$OUT"
cd "$REPO" || exit 2
tools/game_dir_lock.sh acquire coord 1800 >/dev/null || { echo "lock failed"; exit 1; }
trap 'tools/game_dir_lock.sh release coord >/dev/null' EXIT
cp -f build/cmake-debug/reimpl-x64.exe "$G/reimpl-x64.exe" || { echo "exe busy"; exit 1; }
rm -f "$G"/standalone_{debug,error,crash,hang}.log "$G/standalone_boot_state.txt"
( cd "$G" && env BANISHED_AUTOSTART=1 BANISHED_FORCE_FIRE=2 BANISHED_FORCE_FIRE_FRAME=300 BANISHED_FORCE_FIRE_PERIOD=400 BANISHED_PAUSEMENU=1500 BANISHED_PAUSEMENU_QUIT=120 ./reimpl-x64.exe ) & pid=$!
for i in $(seq 1 500); do kill -0 $pid 2>/dev/null || break; sleep 1; done
killed=no; if kill -0 $pid 2>/dev/null; then killed=YES; taskkill //F //PID $pid >/dev/null 2>&1; fi
wait $pid 2>/dev/null; rc=$?
for f in standalone_debug.log standalone_error.log standalone_crash.log standalone_hang.log; do [ -f "$G/$f" ] && cp -f "$G/$f" "$OUT/"; done
echo "route fire: exit $rc crashlog=$(stat -c %s "$G/standalone_crash.log" 2>/dev/null || echo 0) hang=$([ -f "$G/standalone_hang.log" ] && echo YES || echo no) killed=$killed (${i}s)"
cat "$OUT"/standalone_error.log "$OUT"/standalone_debug.log 2>/dev/null | grep -iE '\[Disaster|\[Fire|Ignite|burning' | head -6 | cut -c1-200
