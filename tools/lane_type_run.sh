#!/usr/bin/env bash
# Lane TYPE cutover probe. Renames Runtime-x64.dll aside, runs the DLL-less
# boot, and RESTORES THE DLL unconditionally (trap) -- every other lane's
# oracle needs it back.
set -u
G="/c/GOG Games/Banished"
OUT="${1:?usage: lane_type_run.sh <outdir>}"
mkdir -p "$OUT"

restore() {
  if [ -f "$G/Runtime-x64.dll.absent" ]; then
    mv -f "$G/Runtime-x64.dll.absent" "$G/Runtime-x64.dll"
    echo "[laneTYPE] Runtime-x64.dll RESTORED"
  fi
  if [ -f "$G/Runtime-x64.dll" ]; then
    echo "[laneTYPE] verified present: $(ls -l "$G/Runtime-x64.dll" | awk '{print $5}') bytes"
  else
    echo "[laneTYPE] *** FATAL: Runtime-x64.dll IS MISSING ***"
  fi
}
trap restore EXIT INT TERM

cp -f standalone/build/reimpl-x64.exe "$G/reimpl-x64.exe"
rm -f "$G/standalone_debug.log" "$G/standalone_error.log" "$G/standalone_crash.log"

mv -f "$G/Runtime-x64.dll" "$G/Runtime-x64.dll.absent"
echo "[laneTYPE] DLL moved aside; launching"

cd "$G"
BANISHED_RT_NO_DLL=1 BANISHED_RT_MINT=1 BANISHED_RT_MINT_CTOR=1 \
BANISHED_RT_ALLOC=1 BANISHED_RT_STRICT=1 BANISHED_RT_MINT_SHADOW=1 \
BANISHED_RT_TYPE_SURVEY=1 \
BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120 \
./reimpl-x64.exe &
PID=$!
for i in $(seq 1 60); do
  kill -0 $PID 2>/dev/null || break
  sleep 2
done
kill -9 $PID 2>/dev/null
wait $PID 2>/dev/null
cd - >/dev/null

for f in standalone_debug.log standalone_error.log standalone_crash.log; do
  [ -f "$G/$f" ] && cp -f "$G/$f" "$OUT/$f"
done
echo "[laneTYPE] logs copied to $OUT"
