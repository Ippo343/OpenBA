#!/bin/sh
# Wave 9 lane UIC: deploy + run + copy the logs out, ALL under one game-directory
# lock, because releasing before the copy reintroduces the race the lock exists
# to remove. Two cells per acquisition: the negative control and the clean run,
# so the pair cannot straddle another lane's deploy.
#
# IDENTITY GATE. The on-disk md5 says which exe is on disk when you look, not
# which one ran. The gate here is a line only THIS build can emit: the diff site
# `UI::Element::AdjustUpperLeft` does not exist in any other lane's binary. If it
# is absent from a run's own log, the run is discarded rather than interpreted.
set -u
GAME="C:/GOG Games/Banished"
OUT="docs/runtime_port/wave9_lane_uic"
MINE="standalone/build/reimpl-x64.exe"
mkdir -p "$OUT"

bash tools/game_dir_lock.sh acquire lane-UIC 900 || { echo "lock: FAILED"; exit 2; }

run_cell() {
  label="$1"; shift
  cp "$MINE" "$GAME/reimpl-x64.exe" || { echo "$label: deploy failed"; return 1; }
  rm -f "$GAME/standalone_debug.log" "$GAME/standalone_error.log" \
        "$GAME/standalone_crash.log"
  ( cd "$GAME" && env "$@" ./reimpl-x64.exe ) &
  pid=$!
  w=0
  while kill -0 "$pid" 2>/dev/null; do
    sleep 5; w=$((w+5))
    [ "$w" -ge 400 ] && { taskkill //F //IM reimpl-x64.exe >/dev/null 2>&1; break; }
  done
  wait "$pid" 2>/dev/null
  cp "$GAME/standalone_error.log" "$OUT/${label}_error.log" 2>/dev/null
  cp "$GAME/standalone_debug.log" "$OUT/${label}_debug.log" 2>/dev/null
  if ! grep -q "UI::Element::AdjustUpperLeft" "$OUT/${label}_error.log" 2>/dev/null; then
    echo "$label: IDENTITY GATE FAILED — no AdjustUpperLeft site in this run's own"
    echo "$label: log, so this was NOT my binary. Discarding, not interpreting."
    return 1
  fi
  echo "== $label (identity gate OK, ${w}s)"
  grep -E "AdjustUpperLeft|SetUpperLeft|CreateElement|BringDialogToFront|LinkedList|FreeLocked" \
       "$OUT/${label}_error.log" | grep "calls="
  grep -E "UIC-pop|ReportKeyCollisions|KeyCollision" "$OUT/${label}_error.log" | head -6
  return 0
}

run_cell uicNEG2 BANISHED_AUTOTUTORIAL=1 BANISHED_RT_DIFF=1 \
                 BANISHED_RT_ADJUSTUL_NEGCTL=noborder \
                 BANISHED_PAUSEMENU=6000 BANISHED_PAUSEMENU_QUIT=600
echo
run_cell uicOK2  BANISHED_AUTOTUTORIAL=1 BANISHED_RT_DIFF=1 \
                 BANISHED_PAUSEMENU=6000 BANISHED_PAUSEMENU_QUIT=600

bash tools/game_dir_lock.sh release lane-UIC
