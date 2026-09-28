#!/usr/bin/env bash
# instr.sh <label> : new-game route with the allocator instruments armed (B3R R9).
set -u
G="/c/GOG Games/Banished"; HERE="$(cd "$(dirname "$0")" && pwd)"; OUT="$HERE/runs/$1"; mkdir -p "$OUT"
cd /c/code/GIT/untitled_rev_eng_project
tools/game_dir_lock.sh acquire coord 1800 || exit 1
trap 'tools/game_dir_lock.sh release coord' EXIT
run() { local name="$1"; shift
  cp -f standalone/build/reimpl-x64.exe "$G/reimpl-x64.exe"
  rm -f "$G"/standalone_{debug,error,crash,hang}.log "$G/standalone_boot_state.txt"
  ( cd "$G" && env BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120 "$@" ./reimpl-x64.exe ) & local pid=$!
  local i; for i in $(seq 1 600); do kill -0 $pid 2>/dev/null || break; sleep 1; done
  kill -0 $pid 2>/dev/null && { echo "$name ALIVE, killing"; taskkill //F //IM reimpl-x64.exe >/dev/null 2>&1; }
  wait $pid 2>/dev/null; local rc=$?
  cp -f "$G/standalone_error.log" "$OUT/$name.error.log" 2>/dev/null
  echo "instr $name: exit $rc crashlog=$(stat -c %s "$G/standalone_crash.log" 2>/dev/null || echo 0) (${i}s)"
  grep -E '^\[(ALLOCSTATS|REDZONE)\]' "$OUT/$name.error.log" | grep -E 'armed|rewired|installed' | cut -c1-160
  grep -E '^\[ALLOCSTATS\] live=' "$OUT/$name.error.log" | tail -1 | cut -c1-200
}
run stats BANISHED_ALLOC_STATS=1
run redzone BANISHED_REDZONE=1
run both BANISHED_REDZONE=1 BANISHED_ALLOC_STATS=1
