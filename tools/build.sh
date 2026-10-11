#!/usr/bin/env bash
# tools/build.sh -- the one build / deploy / run entry point over the CMake build (2026-10-11).
#
# usage: tools/build.sh <command> [flavour] [--jobs N]
#   build   [flavour]   configure build/cmake-<flavour> if absent, build reimpl-x64
#   unit    [flavour]   build the unit tests and run them (exit = the suite's exit)
#   deploy  [flavour]   build + copy the exe to the game directory (BANISHED_DIR)
#   run     [flavour]   deploy + launch with BANISHED_AUTOSTART=1 (THE GATE's by-hand run)
#   run-menu [flavour]  deploy + launch, stops at the main menu
#   stress  [flavour]   build perf (default), deploy, tools/stress_run.sh --no-build $STRESS_ARGS
#   clean   [flavour]   remove build/cmake-<flavour> (all four when no flavour is given)
#   exe     [flavour]   print the exe path (for scripts)
#   flavours: debug (default) | og | perf | release   -- CMakeLists.txt's BANISHED_FLAVOUR
#
# Jobs: --jobs N or BUILD_JOBS (default 4: the gate's -j4; agents use 2 -- memory
# feedback_build_parallelism_low_memory). Extra cmake args: CMAKE_EXTRA (e.g. "-DBANISHED_PERF_EXPERIMENTAL=0").
# Toolchain: cmake + ninja from pip on PATH, GCC from C:/msys64/ucrt64/bin (docs/reference/environment.md);
# on Linux the same script works with the POSIX targets (reimpl-x64, unit) -- no deploy / run there
# (tools/linux_gamedir.sh + tools/linux_run.sh do that).
set -u
CMD="${1:-build}"; shift || true
FLAV="debug"; [ "$CMD" = stress ] && FLAV=perf
FLAV_GIVEN=0
case "${1:-}" in debug|og|perf|release) FLAV="$1"; FLAV_GIVEN=1; shift;; esac
JOBS="${BUILD_JOBS:-4}"
while [ $# -gt 0 ]; do
  case "$1" in
    --jobs) JOBS="$2"; shift 2;;
    *) echo "unknown argument $1"; exit 2;;
  esac
done
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) EXT=.exe;; *) EXT="";; esac
BANISHED_DIR="${BANISHED_DIR:-C:/GOG Games/Banished}"
B="build/cmake-$FLAV"
EXE="$B/reimpl-x64$EXT"

configure() {
  [ -f "$B/build.ninja" ] && return 0
  echo "== configure $B (BANISHED_FLAVOUR=$FLAV)"
  cmake -S . -B "$B" -G Ninja -DBANISHED_FLAVOUR="$FLAV" ${CMAKE_EXTRA:-} || exit 1
}
build() { # targets...
  configure
  echo "== build $B: $* (-j$JOBS)"
  cmake --build "$B" -j "$JOBS" -t "$@" || exit 1
}
deploy() {
  build reimpl-x64
  cp -f "$EXE" "$BANISHED_DIR/reimpl-x64.exe" || { echo "deploy: exe busy, is a game running?"; exit 1; }
  rm -f "$BANISHED_DIR/standalone_debug.log"
  echo "Deployed $EXE to $BANISHED_DIR/reimpl-x64.exe"
}

case "$CMD" in
  exe)     echo "$EXE";;
  build)   build reimpl-x64;;
  unit)    build unit; "$B/unit$EXT";;
  deploy)  deploy;;
  run)     deploy; ( cd "$BANISHED_DIR" && BANISHED_AUTOSTART=1 start reimpl-x64.exe ); echo "Launched (autostart)";;
  run-menu) deploy; ( cd "$BANISHED_DIR" && start reimpl-x64.exe ); echo "Launched (menu only)";;
  stress)  deploy; tools/stress_run.sh --no-build ${STRESS_ARGS:-};;
  clean)   if [ "$FLAV_GIVEN" = 1 ]; then rm -rf "$B"; echo "removed $B"
           else rm -rf build/cmake-debug build/cmake-og build/cmake-perf build/cmake-release; echo "removed build/cmake-{debug,og,perf,release}"; fi;;
  *) echo "unknown command $CMD"; sed -n 2,14p "$0"; exit 2;;
esac
