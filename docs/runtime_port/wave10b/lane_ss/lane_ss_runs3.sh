#!/usr/bin/env bash
# Lane SS -- final A/B on the DEFAULT-ON promotion:
#   promoted  = the shipped default (nothing set)
#   killed    = BANISHED_RT_MINT_PROMOTED=0, this file's own kill switch
# plus a shadow pass to record the readiness table on the promoted tip.
set -u
WT="$(pwd)"
G="/c/GOG Games/Banished"
OUT="$WT/lane_ss_logs3"
mkdir -p "$OUT"

cp "$WT/standalone/build/reimpl-x64.exe" "$G/reimpl-x64.exe" || exit 1
md5sum "$WT/standalone/build/reimpl-x64.exe" "$G/reimpl-x64.exe" > "$OUT/exe_md5.txt"

run_one() {
  local label="$1"; shift
  rm -f "$G/standalone_debug.log" "$G/standalone_error.log" "$G/standalone_crash.log"
  ( cd "$G" && env "$@" ./reimpl-x64.exe )
  echo "[lane-SS] $label exit=$?"
  cp "$G/standalone_debug.log" "$OUT/${label}_debug.log" 2>/dev/null
  cp "$G/standalone_error.log" "$OUT/${label}_error.log" 2>/dev/null
  cp "$G/standalone_crash.log" "$OUT/${label}_crash.log" 2>/dev/null
}

run_one promoted BANISHED_AUTOQUIT=300 BANISHED_RT_DIFF=1 BANISHED_RT_SAVE_TRACE=1
run_one killed   BANISHED_AUTOQUIT=300 BANISHED_RT_DIFF=1 BANISHED_RT_SAVE_TRACE=1 \
                 BANISHED_RT_MINT_PROMOTED=0
run_one shadow   BANISHED_AUTOQUIT=300 BANISHED_RT_MINT_SHADOW=1

echo "[lane-SS] done"
