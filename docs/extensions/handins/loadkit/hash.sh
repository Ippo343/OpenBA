#!/usr/bin/env bash
# usage: hash.sh <exe> <seed> <size>  -> prints the [WorldHash] lines of one new game
EXE=$1; SEED=$2; SIZE=$3; G="/c/GOG Games/Banished"
cp -f "$EXE" "$G/reimpl-x64.exe" || exit 2
cd "$G"; rm -f standalone_error.log standalone_crash.log
env BANISHED_AUTOSTART=1 BANISHED_SEED=$SEED BANISHED_MAP_SIZE=$SIZE BANISHED_PAUSEMENU=30 BANISHED_PAUSEMENU_QUIT=60 \
    MSYS2_ARG_CONV_EXCL="/profile:" ./reimpl-x64.exe /profile:faithful >/dev/null 2>&1 &
PID=$!; for i in $(seq 1 1800); do kill -0 $PID 2>/dev/null || break; sleep 0.1; done
kill -0 $PID 2>/dev/null && { echo HANG; kill -9 $PID; }; wait $PID; RC=$?
rm -f /c/Users/USPC/Documents/Banished/Save/Auto.sav   # the quit autosaves; a leftover Auto.sav moves the gate loadgame count
echo "seed=$SEED size=$SIZE rc=$RC crash=$(stat -c %s standalone_crash.log 2>/dev/null) $(grep -h '\[WorldHash\]' standalone_error.log | sed 's/\[WorldHash\] //' | tr -d '\r' | tr '\n' ' ')"
