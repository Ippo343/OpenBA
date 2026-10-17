#!/usr/bin/env bash
# Generated-world comparison (owed at the B.4 close): the SAME seed on two exes, then a diff of the
# pointer-free world-generation / start-town lines. EDIT WITH THE Write TOOL ONLY.
# usage: worldcmp.sh <old-exe> <new-exe> <label> [seed=12345]
# A pixel diff measures nothing (frames are not reproducible); these lines are the world's shape:
# terrain build, river polylines, overlay counts, region graph, start-town spawn pools and positions.
set -u
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"   # this script lives in docs/runtime_port/phase_d/
G="${BANISHED_DIR:-C:/GOG Games/Banished}"; command -v cygpath >/dev/null 2>&1 && G="$(cygpath -u "$G")"   # the game directory (env BANISHED_DIR, as tools/build.sh)
HERE="$(cd "$(dirname "$0")" && pwd)"
OLD="$1"; NEW="$2"; LABEL="$3"; SEED="${4:-12345}"
OUT="$HERE/runs/$LABEL"; mkdir -p "$OUT"
cd "$REPO" || exit 2
tools/game_dir_lock.sh acquire coord 1800 || { echo "lock failed"; exit 1; }
trap 'tools/game_dir_lock.sh release coord' EXIT
run() { # name exe
  cp -f "$2" "$G/reimpl-x64.exe" || { echo "exe busy"; return 1; }
  rm -f "$G"/standalone_{debug,error,crash,hang}.log "$G/standalone_boot_state.txt"
  ( cd "$G" && env BANISHED_AUTOSTART=1 BANISHED_SEED="$SEED" BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120 ./reimpl-x64.exe ) & local pid=$!
  local i; for i in $(seq 1 400); do kill -0 $pid 2>/dev/null || break; sleep 1; done
  if kill -0 $pid 2>/dev/null; then echo "$1: ALIVE after 400s, killing"; taskkill //F //PID $pid >/dev/null 2>&1; fi
  wait $pid 2>/dev/null; local rc=$?
  cat "$G/standalone_error.log" "$G/standalone_debug.log" 2>/dev/null > "$OUT/$1.log"
  echo "$1: exit $rc crashlog=$(stat -c %s "$G/standalone_crash.log" 2>/dev/null || echo 0) (${i}s)"
  grep -E '^\[worldgen|^\[RegionSeed\]|^\[PfSeed\]|^\[startTown|^\[NewGameDialog::StartGame\]|^\[TownCensus\]|^\[GUL::Load\] ENTER' "$OUT/$1.log" \
    | sed -E 's/(0x)?[0-9A-Fa-f]{9,16}/PTR/g; s/overlayVB\(\+0x160\)(\.size)?=[^ ]+/overlayVB=X/' > "$OUT/$1.world.txt"   # pointers of any printed width; the overlay VB line changed what it prints (pointer -> size) during B.4
  echo "$1: $(wc -l < "$OUT/$1.world.txt") world line(s), md5 $(md5sum "$OUT/$1.world.txt" | cut -c1-8)"
}
run old "$OLD"
run new "$NEW"
if diff -q "$OUT/old.world.txt" "$OUT/new.world.txt" > /dev/null; then echo "WORLD: identical ($(wc -l < "$OUT/new.world.txt") lines, seed $SEED)"
else echo "WORLD: DIFFERS"; diff "$OUT/old.world.txt" "$OUT/new.world.txt" | head -40; fi
