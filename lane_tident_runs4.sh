#!/usr/bin/env bash
# Wave 10C lane TIDENT -- the headline claim's effectiveness control.
# CLAIM: UI::SpriteSheet::TypeName's 1 divergence is the PROMOTION handing out
# our node's name pointer. If so, turning the promoted rows OFF must take it to
# zero, while IO::PackageFile::TypeName (never promoted) stays at zero in both.
set -u
G="C:/GOG Games/Banished"
OUT="$(pwd)/lane_tident_logs"
mkdir -p "$OUT"
cp standalone/build/reimpl-x64.exe "$G/reimpl-x64.exe" || exit 1

run() {
  tag="$1"; shift
  rm -f "$G/standalone_debug.log" "$G/standalone_error.log" "$G/standalone_crash.log"
  ( cd "$G" && env BANISHED_AUTOSTART=1 BANISHED_AUTOQUIT=300 \
        BANISHED_RT_DIFF=1 "$@" ./reimpl-x64.exe )
  rc=$?
  mkdir -p "$OUT/$tag"
  for f in standalone_debug.log standalone_error.log standalone_crash.log; do
    cp "$G/$f" "$OUT/$tag/$f" 2>/dev/null || : ; done
  echo "=== $tag exit=$rc crash=$(stat -c%s "$OUT/$tag/standalone_crash.log" 2>/dev/null||echo ?)B ==="
  grep -hE "TypeName " "$OUT/$tag/standalone_error.log" | grep -E "calls=|DIVERGENCE"
}

run promoted_on
run promoted_off BANISHED_RT_MINT_PROMOTED=0
echo ALL_RUNS_DONE
