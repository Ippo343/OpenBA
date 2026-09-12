#!/usr/bin/env bash
# =============================================================================
# tools/pkg2_run.sh -- Wave 9 lane PKG2's deploy+run+capture, under the game lock
# =============================================================================
#
# WHY: six lanes deploy the same reimpl-x64.exe into the one shared game
# directory and write the same logs there. This lane lost three measurements to
# that -- once the logs were overwritten after a good run, twice the exe itself
# was replaced between the copy and the launch (one of those runs reported
# another lane's registration count and would have been believed).
#
# TWO INDEPENDENT PROTECTIONS, because each catches what the other cannot:
#
#  1. tools/game_dir_lock.sh, held across deploy AND run AND the log copy.
#     Releasing before copying the logs out reintroduces the race. The lock is
#     ADVISORY -- it only works if every lane takes it -- which is exactly why
#     protection 2 still exists.
#
#  2. A post-hoc identity check on the LOG, not on the disk. An md5 of the
#     deployed file tells you what is on disk when you look, not what ran; only
#     the log the process itself wrote can tell you that.
#
#     The gate keys on a LANE-OWNED banner, not on a shared total. The first
#     version matched `[RtPort] IO: 77 exports registered` -- then lane MOD
#     landed IO::Modification on the same RtIoPortInit, the IO total moved, and
#     that gate would have failed a perfectly good run. A shared total changes
#     whenever any other lane lands in the same namespace; the banner
#     `[RtPort]   IO::PackageFile (lane PKG2): N exports registered` changes only
#     when this lane's own file changes.
#
# The package set is recorded next to the identity, because for this lane the
# INPUT is as much part of the measurement as the binary: a .pkm appearing in
# WinData changes what LoadPackages mounts even when the exe is right, and the
# lock explicitly does not protect WinData.
#
# USAGE: tools/pkg2_run.sh <label> -- VAR=VAL ...
# =============================================================================
set -u

OUT=docs/runtime_port/wave9_lane_pkg2
GD="${BANISHED_GAME_DIR:-C:/GOG Games/Banished}"
LANE=lane-PKG2
LABEL="${1:?usage: pkg2_run.sh <label> -- ENV=VAL ...}"; shift
[ "${1:-}" = "--" ] && shift

# The expected banner count is derived from THIS build's own source, never
# carried forward as a literal -- a hardcoded number is how the previous gate
# went stale.
WANT=$(grep -c 'RegisterRuntimeExport(' runtime/src/io/package_file.cpp)
echo "[pkg2] expecting IO::PackageFile banner to read $WANT (derived from source)"
# Rung 3 gets its OWN banner in the gate. The rung-1 banner only proves the exe
# is from this LANE; it says nothing about whether it is from this ITERATION,
# and rung 3 is exactly the kind of addition an unrebuilt exe would silently
# lack while still passing the old gate.
WANT3=$(grep -c 'RegisterRuntimeExport(' runtime/src/io/filesystem_openfile_win.cpp)
echo "[pkg2] expecting rung-3 banner to read $WANT3 (derived from source)"

mkdir -p "$OUT"

bash tools/game_dir_lock.sh acquire "$LANE" 900 || {
    echo "[pkg2] could not acquire the game-directory lock"; exit 1; }
trap 'bash tools/game_dir_lock.sh release "$LANE"' EXIT

cp standalone/build/reimpl-x64.exe "$GD/reimpl-x64.exe" || {
    echo "[pkg2] deploy failed (exe busy?) -- another lane may not be taking the lock"
    exit 1; }

echo "[pkg2] pkg set = $(ls "$GD/WinData/" 2>/dev/null | grep -E '\.pkg$|\.pkm$' | tr '\n' ' ')"

tools/rt_run.sh "$LABEL" --timeout 600 -- "$@" > "$OUT/rt_run_${LABEL}_stdout.txt" 2>&1

# Copy the evidence out BEFORE the trap releases the lock.
cp "$GD/standalone_error.log" "$OUT/${LABEL}_error.log" 2>/dev/null
cp "$GD/standalone_debug.log" "$OUT/${LABEL}_debug.log" 2>/dev/null
cp "$GD/standalone_crash.log" "$OUT/${LABEL}_crash.log" 2>/dev/null

GOT=$(grep -oE 'IO::PackageFile \(lane PKG2\): [0-9]+' "$OUT/${LABEL}_debug.log" 2>/dev/null \
      | grep -oE '[0-9]+$')
GOT3=$(grep -oE 'open/read \(lane PKG2 rung 3\): [0-9]+' "$OUT/${LABEL}_debug.log" 2>/dev/null       | grep -oE '[0-9]+$')
if [ "${GOT:-none}" = "$WANT" ] && [ "${GOT3:-none}" = "$WANT3" ]; then
    echo "[pkg2] IDENTITY OK -- the exe that RAN is this lane's ($GOT / rung3 $GOT3 exports)"
    exit 0
fi
echo "[pkg2] IDENTITY FAILED: banners read '${GOT:-absent}'/'${GOT3:-absent}', wanted '$WANT'/'$WANT3'."
echo "[pkg2] The run measured something else. DISCARD IT."
exit 1
