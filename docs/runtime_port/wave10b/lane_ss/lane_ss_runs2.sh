#!/usr/bin/env bash
# Lane SS -- second measurement: WHY does the promoted row's Serialize report
# calls=0?  BANISHED_RT_DIFF=1 gives per-body entry counts, which tells us
# whether our SpriteSheet ctor (the only thing that stamps the minted vtable)
# ever runs at boot.  Run under the game-dir lock.
set -u
WT="$(pwd)"
G="/c/GOG Games/Banished"
OUT="$WT/lane_ss_logs"
mkdir -p "$OUT"

cp "$WT/standalone/build/reimpl-x64.exe" "$G/reimpl-x64.exe" || exit 1

run_one() {
  local label="$1"; shift
  rm -f "$G/standalone_debug.log" "$G/standalone_error.log" "$G/standalone_crash.log"
  ( cd "$G" && env "$@" ./reimpl-x64.exe )
  echo "[lane-SS] $label exit=$?"
  cp "$G/standalone_debug.log" "$OUT/${label}_debug.log" 2>/dev/null
  cp "$G/standalone_error.log" "$OUT/${label}_error.log" 2>/dev/null
  cp "$G/standalone_crash.log" "$OUT/${label}_crash.log" 2>/dev/null
}

run_one diffrows BANISHED_AUTOQUIT=300 BANISHED_RT_DIFF=1 \
                 BANISHED_RT_MINT_ROWS='??_7SpriteSheet@UI@@6B@' \
                 BANISHED_RT_SAVE_TRACE=1

run_one diffctrl BANISHED_AUTOQUIT=300 BANISHED_RT_DIFF=1 \
                 BANISHED_RT_SAVE_TRACE=1

echo "[lane-SS] done"
