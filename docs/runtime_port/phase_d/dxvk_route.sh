#!/usr/bin/env bash
# Phase D.4 RENDER step 1: the DXVK DLL drop-in (plans/RENDER.md section B / D step 1).
# usage: dxvk_route.sh <dxvk-x64-dir> <label> [exe=build/cmake-debug/OpenBA.exe] [newgame|menu|capture|resave]
#   Copies d3d11.dll + dxgi.dll from <dxvk-x64-dir> beside the exe UNDER THE GAME-DIR LOCK, runs one
#   route, collects the DXVK logs, and REMOVES the DLLs on every exit path: the retail exe shares
#   the directory and would silently run on DXVK otherwise.
#     newgame  autostart a new game, pause-menu quit at frame 600 (the gate's newgame route)
#     menu     the main menu only, BANISHED_AUTOQUIT=600 frames
#     capture  newgame under renderdoccmd, one capture at BANISHED_CAPTURE_INGAME=150 (the
#              trace oracle's capture.sh shape), the .rdc moved to runs/<label>/
#     resave   tools/resave_gate/resave_gate.sh Gate, three times, verdicts in runs/<label>/
#   DXVK_* are DXVK's own env vars, read by DXVK, never by our code (CLAUDE.md: no getenv).
# EDIT WITH THE Write TOOL ONLY.
set -u
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"   # this script lives in docs/runtime_port/phase_d/
G="${BANISHED_DIR:-C:/GOG Games/Banished}"; command -v cygpath >/dev/null 2>&1 && G="$(cygpath -u "$G")"   # the game directory (env BANISHED_DIR, as tools/build.sh)
RDCMD="${RENDERDOCCMD:-/c/Program Files/RenderDoc/renderdoccmd.exe}"
HERE="$(cd "$(dirname "$0")" && pwd)"
DXVK="$1"; OUT="$HERE/runs/$2"; EXE="${3:-build/cmake-debug/OpenBA.exe}"; MODE="${4:-newgame}"
mkdir -p "$OUT"
cd "$REPO" || exit 2
[ -f "$DXVK/d3d11.dll" ] && [ -f "$DXVK/dxgi.dll" ] || { echo "no d3d11.dll / dxgi.dll in $DXVK"; exit 2; }
for f in d3d11.dll dxgi.dll; do [ -e "$G/$f" ] && { echo "$G/$f ALREADY EXISTS: refusing (an earlier run left it?)"; exit 2; }; done
tools/game_dir_lock.sh acquire coord 1800 >/dev/null || { echo "lock failed"; exit 1; }
cleanup() {
  rm -f "$G/d3d11.dll" "$G/dxgi.dll"
  for f in "$G"/OpenBA_d3d11.log "$G"/OpenBA_dxgi.log "$G"/OpenBA_d3d10core.log; do [ -f "$f" ] && mv -f "$f" "$OUT/"; done
  tools/game_dir_lock.sh release coord >/dev/null
  ls "$G"/d3d11.dll "$G"/dxgi.dll >/dev/null 2>&1 && echo "!! DXVK DLLS STILL IN $G"
}
trap cleanup EXIT
cp -f "$DXVK/d3d11.dll" "$DXVK/dxgi.dll" "$G/" || { echo "dll copy failed"; exit 1; }
sha256sum "$DXVK/d3d11.dll" "$DXVK/dxgi.dll" > "$OUT/dxvk_sha256.txt"

if [ "$MODE" = resave ]; then
  for n in 1 2 3; do
    DXVK_LOG_LEVEL=info bash tools/resave_gate/resave_gate.sh Gate "$OUT/resave$n" 400 > "$OUT/resave$n.out" 2>&1
    echo "resave $n: $(grep -E 'RESULT=|PASS|DIFF' "$OUT/resave$n.out" | tail -1 | cut -c1-160)"
  done
  exit 0
fi

cp -f "$EXE" "$G/OpenBA.exe" || { echo "exe busy"; exit 1; }
rm -f "$G"/standalone_{debug,error,crash,hang}.log "$G/standalone_boot_state.txt" "$G"/reimpl_capture*.rdc
case "$MODE" in
  menu)
    ( cd "$G" && env DXVK_LOG_LEVEL=info BANISHED_AUTOQUIT=600 ./OpenBA.exe ) & pid=$! ;;
  capture)
    ( cd "$G" && env DXVK_LOG_LEVEL=info BANISHED_AUTOSTART=1 BANISHED_CAPTURE_INGAME=150 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120 \
        "$RDCMD" capture --opt-ref-all-resources -d "$G" -c "$G/reimpl_capture" "$G/OpenBA.exe" ) > "$OUT/renderdoccmd.out" 2>&1 & pid=$! ;;
  *)
    ( cd "$G" && env DXVK_LOG_LEVEL=info BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120 ./OpenBA.exe ) & pid=$! ;;
esac
gamepid() { tasklist //FI "IMAGENAME eq OpenBA.exe" //FO CSV 2>/dev/null | grep -i OpenBA | head -1 | cut -d, -f2 | tr -d '"'; }
if [ "$MODE" = capture ]; then
  # renderdoccmd returns right after launching the game (2026-10-10: the first capture run was
  # orphaned by waiting on it); wait on the game's own Windows PID instead.
  sleep 5; gp=$(gamepid)
  for i in $(seq 1 540); do [ -n "$gp" ] || break; tasklist //FI "PID eq $gp" 2>/dev/null | grep -qi OpenBA || break; sleep 1; done
else
  for i in $(seq 1 540); do kill -0 $pid 2>/dev/null || break; sleep 1; done
fi
killed=no
if kill -0 $pid 2>/dev/null; then
  killed=YES
  gp=$(tasklist //FI "IMAGENAME eq OpenBA.exe" //FO CSV 2>/dev/null | grep -i OpenBA | head -1 | cut -d, -f2 | tr -d '"')
  [ -n "$gp" ] && taskkill //F //PID "$gp" >/dev/null 2>&1
  taskkill //F //PID $pid >/dev/null 2>&1
fi
wait $pid 2>/dev/null; rc=$?
for f in standalone_debug.log standalone_error.log standalone_crash.log standalone_hang.log standalone_boot_state.txt; do [ -f "$G/$f" ] && cp -f "$G/$f" "$OUT/"; done
for f in "$G"/reimpl_capture*.rdc; do [ -f "$f" ] && mv -f "$f" "$OUT/"; done
echo "dxvk route $MODE: exit $rc crashlog=$(stat -c %s "$G/standalone_crash.log" 2>/dev/null || echo 0) hang=$([ -f "$G/standalone_hang.log" ] && echo YES || echo no) killed=$killed (${i}s)"
cat "$OUT"/standalone_error.log "$OUT"/standalone_debug.log 2>/dev/null | grep -E "OnCommand\] -> App::Quit|\[AutoStart\]|\[ExtLoad\] calls=|\[BootState\]|\[RenderDoc\]|AutoQuit" | head -8 | cut -c1-200
ls "$OUT"/*.rdc 2>/dev/null
