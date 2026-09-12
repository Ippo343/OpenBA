#!/usr/bin/env bash
# =============================================================================
# tools/pkg3_run.sh -- Wave 9 lane PKG3's deploy+run+capture, under the game lock
# =============================================================================
#
# tools/pkg2_run.sh with a THIRD banner in the gate. Everything in its header
# still applies and is not repeated here; the one addition is the reason for the
# third line.
#
# PKG2's gate keys on two lane-owned banners (IO::PackageFile, and rung 3's
# open/read). Both are satisfied by an exe built BEFORE this lane's file existed,
# because neither changes when package_file_read_win.cpp is added. That is
# exactly the failure the rung-3 banner was itself added to catch -- an
# unrebuilt exe silently lacking the new registration while passing the old
# gate -- so the same fix applies one rung further on.
#
# All three expected counts are derived from THIS build's own source. A
# hardcoded number is how the previous gate went stale.
#
# USAGE: tools/pkg3_run.sh <label> -- VAR=VAL ...
# =============================================================================
set -u

OUT=docs/runtime_port/wave9_lane_pkg3
GD="${BANISHED_GAME_DIR:-C:/GOG Games/Banished}"
LANE=lane-PKG3
LABEL="${1:?usage: pkg3_run.sh <label> -- ENV=VAL ...}"; shift
[ "${1:-}" = "--" ] && shift

WANT=$(grep -c 'RegisterRuntimeExport(' runtime/src/io/package_file.cpp)
WANT3=$(grep -c 'RegisterRuntimeExport(' runtime/src/io/filesystem_openfile_win.cpp)
# ANCHORED, unlike the two above: package_file_read_win.cpp's registration block
# carries a COMMENT that names `RegisterRuntimeExport("` (explaining why the name
# must be a literal), and an unanchored grep counts that comment as a second
# registration and fails a perfectly good run. The anchor counts statements.
WANT4=$(grep -cE '^ *RegisterRuntimeExport\(' runtime/src/io/package_file_read_win.cpp)
echo "[pkg3] expecting banners: PackageFile=$WANT rung3=$WANT3 ReadPackfile=$WANT4 (all derived from source)"

mkdir -p "$OUT"

bash tools/game_dir_lock.sh acquire "$LANE" 900 || {
    echo "[pkg3] could not acquire the game-directory lock"; exit 1; }
trap 'bash tools/game_dir_lock.sh release "$LANE"' EXIT

cp standalone/build/reimpl-x64.exe "$GD/reimpl-x64.exe" || {
    echo "[pkg3] deploy failed (exe busy?) -- another lane may not be taking the lock"
    exit 1; }

# The package set is part of the measurement: a .pkm appearing in WinData changes
# what LoadPackages mounts even when the exe is right, and the lock does not
# protect WinData.
echo "[pkg3] pkg set = $(ls "$GD/WinData/" 2>/dev/null | grep -E '\.pkg$|\.pkm$' | tr '\n' ' ')"

tools/rt_run.sh "$LABEL" --timeout 600 -- "$@" > "$OUT/rt_run_${LABEL}_stdout.txt" 2>&1

# Copy the evidence out BEFORE the trap releases the lock.
cp "$GD/standalone_error.log" "$OUT/${LABEL}_error.log" 2>/dev/null
cp "$GD/standalone_debug.log" "$OUT/${LABEL}_debug.log" 2>/dev/null
cp "$GD/standalone_crash.log" "$OUT/${LABEL}_crash.log" 2>/dev/null

GOT=$(grep -oE 'IO::PackageFile \(lane PKG2\): [0-9]+' "$OUT/${LABEL}_debug.log" 2>/dev/null | grep -oE '[0-9]+$')
GOT3=$(grep -oE 'open/read \(lane PKG2 rung 3\): [0-9]+' "$OUT/${LABEL}_debug.log" 2>/dev/null | grep -oE '[0-9]+$')
GOT4=$(grep -oE 'ReadPackfile \(lane PKG3\): [0-9]+' "$OUT/${LABEL}_debug.log" 2>/dev/null | grep -oE '[0-9]+$')
if [ "${GOT:-none}" = "$WANT" ] && [ "${GOT3:-none}" = "$WANT3" ] && [ "${GOT4:-none}" = "$WANT4" ]; then
    echo "[pkg3] IDENTITY OK -- the exe that RAN is this lane's ($GOT / rung3 $GOT3 / readpackfile $GOT4)"
    exit 0
fi
echo "[pkg3] IDENTITY FAILED: banners read '${GOT:-absent}'/'${GOT3:-absent}'/'${GOT4:-absent}', wanted '$WANT'/'$WANT3'/'$WANT4'."
echo "[pkg3] The run measured something else. DISCARD IT."
exit 1
