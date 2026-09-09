#!/usr/bin/env bash
# Wave 10C lane TIDENT -- hunt for ANY route that enters TypeIdOfStatic, i.e.
# that gives ?_typeInfo@DialogController@ a live consumer. Panel routes are the
# candidates: AddControllerTab is what a TABBED dialog uses.
set -u
G="C:/GOG Games/Banished"
OUT="$(pwd)/lane_tident_logs"
mkdir -p "$OUT"
cp standalone/build/reimpl-x64.exe "$G/reimpl-x64.exe" || exit 1

run() {
  tag="$1"; shift
  rm -f "$G/standalone_debug.log" "$G/standalone_error.log" "$G/standalone_crash.log"
  ( cd "$G" && env BANISHED_AUTOSTART=1 BANISHED_AUTOQUIT=600 \
        BANISHED_RT_MINT_SHADOW=1 "$@" ./reimpl-x64.exe )
  rc=$?
  mkdir -p "$OUT/$tag"
  for f in standalone_debug.log standalone_error.log standalone_crash.log; do
    cp "$G/$f" "$OUT/$tag/$f" 2>/dev/null || : ; done
  n=$(grep -hc "TidStatic" "$OUT/$tag/standalone_debug.log" 2>/dev/null || echo 0)
  echo "=== $tag exit=$rc  [TidStatic] lines=$n ==="
  grep -h "TidStatic" "$OUT/$tag/standalone_debug.log" 2>/dev/null | sort -u
}

run route_housepanel BANISHED_TOOL_HARNESS=housepanel
run route_townhall   BANISHED_TOOL_HARNESS=pinpanel
run route_tradepanel BANISHED_TOOL_HARNESS=tradepanel
echo ALL_RUNS_DONE
