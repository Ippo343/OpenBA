#!/usr/bin/env bash
# Wave 10C lane TIDENT -- the three measurement runs, under ONE lock.
#   1 control   : ptr= screen as shipped, plus the RT_DIFF calls= columns
#   2 ptralias  : the effectiveness control -- ptr= must drop by exactly 5
#   3 candidate : ?_typeInfo@DialogController@ armed via BANISHED_RT_MINT_ROWS
set -u
G="C:/GOG Games/Banished"
OUT="$(pwd)/lane_tident_logs"
mkdir -p "$OUT"
cp standalone/build/reimpl-x64.exe "$G/reimpl-x64.exe" || exit 1

run() {                       # run <tag> <extra env assignments...>
  tag="$1"; shift
  rm -f "$G/standalone_debug.log" "$G/standalone_error.log" "$G/standalone_crash.log"
  ( cd "$G" && env BANISHED_AUTOSTART=1 BANISHED_AUTOQUIT=300 \
        BANISHED_RT_MINT_SHADOW=1 BANISHED_RT_DIFF=1 "$@" ./reimpl-x64.exe )
  rc=$?
  mkdir -p "$OUT/$tag"
  for f in standalone_debug.log standalone_error.log standalone_crash.log; do
    cp "$G/$f" "$OUT/$tag/$f" 2>/dev/null || : ; done
  echo "$rc" > "$OUT/$tag/exit_code.txt"
  echo "=== $tag exit=$rc  crash=$(stat -c%s "$OUT/$tag/standalone_crash.log" 2>/dev/null || echo missing) bytes ==="
}

run control
run ptralias BANISHED_RT_MINT_PTRALIAS=5
run candidate "BANISHED_RT_MINT_ROWS=?_typeInfo@DialogController@UI@@0VBaseTypeInfo@Object@@A"
echo ALL_RUNS_DONE
