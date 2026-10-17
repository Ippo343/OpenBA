#!/usr/bin/env bash
# usage: hash.sh <exe> <seed> <size>  -> prints the [WorldHash] lines of one new game
EXE=$1; SEED=$2; SIZE=$3
G="${BANISHED_DIR:-C:/GOG Games/Banished}"; command -v cygpath >/dev/null 2>&1 && G="$(cygpath -u "$G")"   # the game directory (env BANISHED_DIR, as tools/build.sh)
# The save directory: Documents/Banished/Save under the Windows profile, the Linux build's
# ~/.local/share/Banished/Save when USERPROFILE is unset; BANISHED_SAVE_DIR overrides (as resave_gate.sh).
if [ -n "${BANISHED_SAVE_DIR:-}" ]; then S="$BANISHED_SAVE_DIR"
elif [ -n "${USERPROFILE:-}" ]; then S="$USERPROFILE/Documents/Banished/Save"
else S="$HOME/.local/share/Banished/Save"; fi
cp -f "$EXE" "$G/OpenBA.exe" || exit 2
cd "$G"; rm -f standalone_error.log standalone_crash.log
env BANISHED_AUTOSTART=1 BANISHED_SEED=$SEED BANISHED_MAP_SIZE=$SIZE BANISHED_PAUSEMENU=30 BANISHED_PAUSEMENU_QUIT=60 \
    MSYS2_ARG_CONV_EXCL="/profile:" ./OpenBA.exe /profile:faithful >/dev/null 2>&1 &
PID=$!; for i in $(seq 1 1800); do kill -0 $PID 2>/dev/null || break; sleep 0.1; done
kill -0 $PID 2>/dev/null && { echo HANG; kill -9 $PID; }; wait $PID; RC=$?
rm -f "$S/Auto.sav"   # the quit autosaves; a leftover Auto.sav moves the gate loadgame count
echo "seed=$SEED size=$SIZE rc=$RC crash=$(stat -c %s standalone_crash.log 2>/dev/null) $(grep -h '\[WorldHash\]' standalone_error.log | sed 's/\[WorldHash\] //' | tr -d '\r' | tr '\n' ' ')"
