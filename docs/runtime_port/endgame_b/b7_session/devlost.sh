#!/usr/bin/env bash
# Forced device-lost round trip in a running game (frame 2000: after the world is built — the =400
# window is the OPEN loader race, DEVICE_LOST_LOADER_RACE.md, not a gate). usage: devlost.sh <label>
# EDIT WITH THE Write TOOL ONLY.
set -u
REPO=/c/code/GIT/untitled_rev_eng_project
G="/c/GOG Games/Banished"
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$HERE/runs/$1"; mkdir -p "$OUT"
cd "$REPO" || exit 2
tools/game_dir_lock.sh acquire coord 1800 || { echo "lock failed"; exit 1; }
trap 'tools/game_dir_lock.sh release coord' EXIT
cp -f standalone/build/reimpl-x64.exe "$G/reimpl-x64.exe" || { echo "exe busy"; exit 1; }
rm -f "$G"/standalone_{debug,error,crash,hang}.log "$G/standalone_boot_state.txt"
( cd "$G" && env BANISHED_AUTOSTART=1 BANISHED_FORCE_DEVICE_LOST=${DLFRAME:-12000} BANISHED_PAUSEMENU=${DLPAUSE:-20000} BANISHED_PAUSEMENU_QUIT=120 ./reimpl-x64.exe ) & pid=$!
# S4 (2026-10-05): after ONE injection the device stays removed (hr 0x887a0005 on every later Present) and
# the exe runs a round trip per frame, about 100 MB each: 200 round trips and 20 GB of working set by the
# 600 s kill. That is what tripped the harness's low-memory stop twice (known_port_issues row 890). The
# watch therefore ends the run once DLMAX (default 5) round trips are logged; killed=YES is then expected.
for i in $(seq 1 600); do
  kill -0 $pid 2>/dev/null || break
  [ "$(cat "$G/standalone_error.log" "$G/standalone_debug.log" 2>/dev/null | grep -c 'device-lost round trip COMPLETE')" -ge "${DLMAX:-5}" ] && break
  sleep 1
done
killed=no; if kill -0 $pid 2>/dev/null; then killed=YES; taskkill //F //IM reimpl-x64.exe >/dev/null 2>&1; fi
wait $pid 2>/dev/null; rc=$?
for f in standalone_debug.log standalone_error.log standalone_crash.log standalone_hang.log; do [ -f "$G/$f" ] && cp -f "$G/$f" "$OUT/$f"; done
echo "devlost: exit $rc crashlog=$(stat -c %s "$G/standalone_crash.log" 2>/dev/null || echo 0) hang=$([ -f "$G/standalone_hang.log" ] && echo YES || echo no) killed=$killed (${i}s) INJECT=$(cat "$OUT"/*.log | grep -c 'INJECTING DXGI_ERROR_DEVICE_RESET') COMPLETE=$(cat "$OUT"/*.log | grep -c 'device-lost round trip COMPLETE') TRAP=$(cat "$OUT"/*.log | grep -c '\[RtMint\] TRAP')"
