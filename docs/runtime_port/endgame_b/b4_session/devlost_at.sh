#!/usr/bin/env bash
# Build a given commit in a short-path throwaway worktree and run the forced device-lost route on it.
# usage: devlost_at.sh <commit> <label> [frame=2000]      EDIT WITH THE Write TOOL ONLY.
set -u
REPO=/c/code/GIT/untitled_rev_eng_project
G="/c/GOG Games/Banished"
HERE="$(cd "$(dirname "$0")" && pwd)"
C="$1"; L="$2"; F="${3:-2000}"
WT="$REPO/.claude/worktrees/dl"
MK=/c/msys64/ucrt64/bin/mingw32-make.exe
OUT="$HERE/runs/$L"; mkdir -p "$OUT"
cd "$REPO" || exit 2
if [ -d "$WT" ]; then ( cd "$WT" && git checkout -q --detach "$C" ) || exit 2
else git worktree add --detach "$WT" "$C" > "$OUT/wt.log" 2>&1 || { echo "worktree failed"; exit 2; }; fi
cd "$WT" || exit 2
$MK -j4 standalone-debug > "$OUT/build.log" 2>&1 || { echo "$L: build failed"; tail -5 "$OUT/build.log"; exit 1; }
git checkout -q -- . 2>/dev/null
tools/game_dir_lock.sh acquire coord 1800 || { echo "lock failed"; exit 1; }
trap 'tools/game_dir_lock.sh release coord >/dev/null' EXIT
for n in 1; do
  cp -f standalone/build/reimpl-x64.exe "$G/reimpl-x64.exe" || { echo "exe busy"; exit 1; }
  rm -f "$G"/standalone_{debug,error,crash,hang}.log "$G/standalone_boot_state.txt"
  ( cd "$G" && env BANISHED_AUTOSTART=1 BANISHED_FORCE_DEVICE_LOST=$F BANISHED_PAUSEMENU=$((F+8000)) BANISHED_PAUSEMENU_QUIT=120 ./reimpl-x64.exe ) & pid=$!
  for i in $(seq 1 420); do kill -0 $pid 2>/dev/null || break; sleep 1; done
  killed=no; if kill -0 $pid 2>/dev/null; then killed=YES; taskkill //F //IM reimpl-x64.exe >/dev/null 2>&1; fi
  wait $pid 2>/dev/null; rc=$?
  mkdir -p "$OUT/$n"
  for f in standalone_debug.log standalone_error.log standalone_crash.log standalone_hang.log; do [ -f "$G/$f" ] && cp -f "$G/$f" "$OUT/$n/$f"; done
  echo "$L @ $(git rev-parse --short HEAD) frame=$F run $n: exit $rc crashlog=$(stat -c %s "$G/standalone_crash.log" 2>/dev/null || echo 0) killed=$killed (${i}s) INJECT=$(cat "$OUT/$n"/*.log | grep -c 'INJECTING DXGI') COMPLETE=$(cat "$OUT/$n"/*.log | grep -c 'round trip COMPLETE') fault=$(grep -m1 '^Fault' "$OUT/$n/standalone_crash.log" 2>/dev/null)"
done
