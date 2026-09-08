#!/bin/sh
# Wave 9 lane UIC scratch: six lanes deploy into ONE game directory, so the gap
# between "no reimpl running" and "my exe is deployed" is a race another lane
# wins about half the time. This closes the gap: poll, deploy the instant the
# directory frees, verify the deployed exe is MINE by size, and only then run.
# A measurement taken on another lane's exe is worse than no measurement.
set -u
LABEL="$1"; shift
GAME="C:/GOG Games/Banished"
MINE="standalone/build/reimpl-x64.exe"

i=0
while [ "$i" -lt 240 ]; do
  if ! tasklist 2>/dev/null | grep -qi 'reimpl-x64\.exe'; then
    if cp "$MINE" "$GAME/reimpl-x64.exe" 2>/dev/null; then
      break
    fi
  fi
  sleep 5
  i=$((i+1))
done

a=$(stat -c %s "$MINE")
b=$(stat -c %s "$GAME/reimpl-x64.exe" 2>/dev/null || echo 0)
if [ "$a" != "$b" ]; then
  echo "uic_run[$LABEL]: DEPLOY LOST THE RACE (mine=$a deployed=$b) — refusing to measure"
  exit 2
fi
echo "uic_run[$LABEL]: deployed OK, size=$a"
exec tools/rt_run.sh "$LABEL" "$@"
