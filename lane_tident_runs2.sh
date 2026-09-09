#!/usr/bin/env bash
# Wave 10C lane TIDENT -- the DialogController promotion A/B, on the [TidStatic]
# attendance line. control = row NOT registered; armed = row registered.
set -u
G="C:/GOG Games/Banished"
OUT="$(pwd)/lane_tident_logs"
mkdir -p "$OUT"
cp standalone/build/reimpl-x64.exe "$G/reimpl-x64.exe" || exit 1

run() {
  tag="$1"; shift
  rm -f "$G/standalone_debug.log" "$G/standalone_error.log" "$G/standalone_crash.log"
  ( cd "$G" && env BANISHED_AUTOSTART=1 BANISHED_AUTOQUIT=300 \
        BANISHED_RT_MINT_SHADOW=1 BANISHED_RT_DIFF=1 "$@" ./reimpl-x64.exe )
  rc=$?
  mkdir -p "$OUT/$tag"
  for f in standalone_debug.log standalone_error.log standalone_crash.log; do
    cp "$G/$f" "$OUT/$tag/$f" 2>/dev/null || : ; done
  echo "$rc" > "$OUT/$tag/exit_code.txt"
  echo "=== $tag exit=$rc crash=$(stat -c%s "$OUT/$tag/standalone_crash.log" 2>/dev/null || echo ?) bytes ==="
  grep -h "TidStatic" "$OUT/$tag/standalone_debug.log" | sort -u
}

run tid_control
run tid_armed "BANISHED_RT_MINT_ROWS=?_typeInfo@DialogController@UI@@0VBaseTypeInfo@Object@@A"
echo ALL_RUNS_DONE
