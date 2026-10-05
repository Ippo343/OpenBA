#!/usr/bin/env bash
# UI routes for a merge that touched UI classes: front-end Quit / Options / Achievements, GoalDialog from a
# tutorial game's pause menu, Mods with .pkm. usage: uiroutes.sh <label>   EDIT WITH THE Write TOOL ONLY.
set -u
REPO=/c/code/GIT/untitled_rev_eng_project
G="/c/GOG Games/Banished"
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$HERE/runs/$1"; mkdir -p "$OUT"
SUM="$OUT/summary.txt"; : > "$SUM"
say() { echo "$*" | tee -a "$SUM"; }
cd "$REPO" || exit 2
say "== uiroutes $1 @ $(git rev-parse --short HEAD) $(date +%H:%M:%S)"
bash "$HERE/mods_route.sh" "$1_mods" > "$OUT/mods.log" 2>&1
say "mods route: $(grep -E '^exit=|crashlog bytes|^hang' "$OUT/mods.log" | tr '\n' ' ') $(grep -h 'RtResolve\] ' "$OUT/mods.log" | tail -1)"
tools/game_dir_lock.sh acquire coord 1800 || { say "lock failed"; exit 1; }
trap 'tools/game_dir_lock.sh release coord' EXIT
route() { # name secs env...
  local name="$1" secs="$2"; shift 2
  cp -f standalone/build/reimpl-x64.exe "$G/reimpl-x64.exe" || { say "  !! route $name: exe busy"; return 1; }
  rm -f "$G"/standalone_{debug,error,crash,hang}.log "$G/standalone_boot_state.txt"
  ( cd "$G" && env "$@" ./reimpl-x64.exe ) & local pid=$!
  local i; for i in $(seq 1 "$secs"); do kill -0 $pid 2>/dev/null || break; sleep 1; done
  local killed=no
  if kill -0 $pid 2>/dev/null; then killed=YES; taskkill //F //IM reimpl-x64.exe >/dev/null 2>&1; fi
  wait $pid 2>/dev/null; local rc=$?
  mkdir -p "$OUT/$name"
  for f in standalone_debug.log standalone_error.log standalone_crash.log standalone_hang.log; do
    [ -f "$G/$f" ] && cp -f "$G/$f" "$OUT/$name/$f"
  done
  local crash; crash=$(stat -c %s "$G/standalone_crash.log" 2>/dev/null || echo 0)
  local hang=no; [ -f "$G/standalone_hang.log" ] && hang=YES
  say "route $name: exit $rc crashlog=$crash hang=$hang killed=$killed (${i}s) TRAP=$(cat "$OUT/$name"/*.log 2>/dev/null | grep -c '\[RtMint\] TRAP') $(cat "$OUT/$name"/*.log 2>/dev/null | grep '\[RtResolve\]' | grep -i miss | tail -1 | cut -c1-40)"
}
route menuquit     300 BANISHED_AUTOMENU_BUTTON=6
route options      300 BANISHED_AUTOMENU_BUTTON=4 BANISHED_AUTOMENU_CLICKS=m6 BANISHED_MODAL_ANSWER=1
route achievements 300 BANISHED_AUTOMENU_BUTTON=8 BANISHED_AUTOMENU_CLICKS=m6 BANISHED_MODAL_ANSWER=1
route goaldialog   200 BANISHED_AUTOTUTORIAL=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=240 BANISHED_PAUSEMENU_BUTTON=10
say "  goaldialog (killed=YES exit 1 is expected): $(cat "$OUT/goaldialog"/standalone_*.log 2>/dev/null | grep -c -- '-> OpenGoal (GoalDialog)') OpenGoal line(s)"
say "== done $(date +%H:%M:%S)"
