#!/usr/bin/env bash
# Lane SS -- three boots inside ONE game-dir lock. Run via
#   tools/game_dir_lock.sh run lane-SS 1800 --snapshot ./lane_ss_logs -- ./lane_ss_runs.sh
set -u
WT="$(pwd)"
G="/c/GOG Games/Banished"
OUT="$WT/lane_ss_logs"
mkdir -p "$OUT"

cp "$WT/standalone/build/reimpl-x64.exe" "$G/reimpl-x64.exe" || exit 1
md5sum "$WT/standalone/build/reimpl-x64.exe" "$G/reimpl-x64.exe" > "$OUT/exe_md5.txt"

run_one() {
  local label="$1"; shift
  rm -f "$G/standalone_debug.log" "$G/standalone_error.log" "$G/standalone_crash.log"
  ( cd "$G" && env "$@" ./reimpl-x64.exe )
  local rc=$?
  echo "[lane-SS] $label exit=$rc"
  cp "$G/standalone_debug.log" "$OUT/${label}_debug.log" 2>/dev/null
  cp "$G/standalone_error.log" "$OUT/${label}_error.log" 2>/dev/null
  cp "$G/standalone_crash.log" "$OUT/${label}_crash.log" 2>/dev/null
  echo "$rc" > "$OUT/${label}_exit.txt"
}

# (a) shadow readiness gate -- shadow only, nothing registered
run_one shadow  BANISHED_AUTOQUIT=300 BANISHED_RT_MINT_SHADOW=1

# control half of the A/B -- nothing extra at all
run_one control BANISHED_AUTOQUIT=300

# (b) the candidate row registered, plus the write-path oracle
run_one rows    BANISHED_AUTOQUIT=300 \
                BANISHED_RT_MINT_ROWS='??_7SpriteSheet@UI@@6B@' \
                BANISHED_RT_SAVE_TRACE=1

echo "[lane-SS] all three runs done"
