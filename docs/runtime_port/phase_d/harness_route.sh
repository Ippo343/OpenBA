#!/usr/bin/env bash
# One BANISHED_TOOL_HARNESS route on the debug exe (pick / panelclose added by B8 RES; any name the
# harness knows). usage: harness_route.sh <harness-name> <label>   EDIT WITH THE Write TOOL ONLY.
set -u
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"   # this script lives in docs/runtime_port/phase_d/
G="${BANISHED_DIR:-C:/GOG Games/Banished}"; command -v cygpath >/dev/null 2>&1 && G="$(cygpath -u "$G")"   # the game directory (env BANISHED_DIR, as tools/build.sh)
HERE="$(cd "$(dirname "$0")" && pwd)"
NAME="$1"; OUT="$HERE/runs/$2"; mkdir -p "$OUT"
cd "$REPO" || exit 2
tools/game_dir_lock.sh acquire coord 1800 >/dev/null || { echo "lock failed"; exit 1; }
trap 'tools/game_dir_lock.sh release coord >/dev/null' EXIT
cp -f build/cmake-debug/OpenBA.exe "$G/OpenBA.exe" || { echo "exe busy"; exit 1; }
rm -f "$G"/standalone_{debug,error,crash,hang}.log "$G/standalone_boot_state.txt"
( cd "$G" && env BANISHED_AUTOSTART=1 BANISHED_TOOL_HARNESS="$NAME" BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=120 ./OpenBA.exe ) & pid=$!
for i in $(seq 1 500); do kill -0 $pid 2>/dev/null || break; sleep 1; done
killed=no; if kill -0 $pid 2>/dev/null; then killed=YES; taskkill //F //PID $pid >/dev/null 2>&1; fi
wait $pid 2>/dev/null; rc=$?
for f in standalone_debug.log standalone_error.log standalone_crash.log standalone_hang.log; do [ -f "$G/$f" ] && cp -f "$G/$f" "$OUT/"; done
echo "route $NAME: exit $rc crashlog=$(stat -c %s "$G/standalone_crash.log" 2>/dev/null || echo 0) hang=$([ -f "$G/standalone_hang.log" ] && echo YES || echo no) killed=$killed (${i}s)"
cat "$OUT"/standalone_error.log "$OUT"/standalone_debug.log 2>/dev/null | grep -E "\[ToolHarness\]\[$NAME\]" | head -8 | cut -c1-220
