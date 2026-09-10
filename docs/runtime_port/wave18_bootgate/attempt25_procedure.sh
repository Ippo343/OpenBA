#!/usr/bin/env bash
# Wave 18 lane GATE -- one controlled cutover boot attempt, per
# docs/runtime_port/WAVE9_CUTOVER_BOOT_RUNBOOK.md.
# Invoked under tools/game_dir_lock.sh run, so the lock is self-releasing.
set -u

WT="C:/code/GIT/wt-w18-gate"
GD="C:/GOG Games/Banished"
ARC="$WT/docs/runtime_port/wave18_bootgate"
mkdir -p "$ARC"

echo "=== assert-held ==="
"$WT/tools/game_dir_lock.sh" assert-held gate
if [ $? -ne 0 ]; then echo "LOCK NOT HELD -- ABORT"; exit 9; fi

echo "=== no reimpl running ==="
tasklist | grep -i reimpl
echo "(above should be empty)"

echo "=== deploy ==="
cp "$WT/standalone/build/reimpl-x64.exe" "$GD/reimpl-x64.exe" || exit 8
ls -l --time-style=full-iso "$GD/reimpl-x64.exe"

echo "=== move DLL aside ==="
cd "$GD" || exit 8
mv Runtime-x64.dll Runtime-x64.dll.absent || exit 8
ls -l Runtime-x64.dll*

echo "=== boot ==="
rm -f standalone_debug.log standalone_error.log standalone_crash.log standalone_boot_state.txt
BANISHED_RT_NO_DLL=1 \
BANISHED_RT_MINT=1 \
BANISHED_RT_MINT_CTOR=1 \
BANISHED_RT_ALLOC=1 \
BANISHED_RT_STRICT=1 \
BANISHED_RT_MINT_SHADOW=1 \
BANISHED_AUTOSTART=1 \
timeout 240 ./reimpl-x64.exe
echo "EXITCODE_OF_RUN=$?"

echo "=== restore DLL (non-negotiable) ==="
mv Runtime-x64.dll.absent Runtime-x64.dll
ls -l Runtime-x64.dll*

echo "=== archive ==="
cp -f standalone_error.log "$ARC/attempt25_stderr.txt" 2>/dev/null
cp -f standalone_crash.log "$ARC/attempt25_crash.txt" 2>/dev/null
cp -f standalone_debug.log "$ARC/attempt25_debug.txt" 2>/dev/null
cp -f standalone_boot_state.txt "$ARC/attempt25_boot_state.txt" 2>/dev/null
wc -l "$ARC"/attempt25_*
echo "=== done ==="
