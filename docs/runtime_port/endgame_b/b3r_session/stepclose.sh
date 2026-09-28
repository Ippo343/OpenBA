#!/usr/bin/env bash
# B.3.8 step-close routes. Run AFTER the last gate.sh has released the lock. EDIT WITH THE Write TOOL ONLY.
# usage: stepclose.sh   (writes runs/stepclose/summary.txt)
# Step-7 additions over b36_session/stepclose.sh: the GOALS front-end route is dropped (id 10 is a
# pause-menu-only button; menu-driver skill), an in-game SAVE (resave gate file mode) is added, and
# the closeclean new-game log is checked for Audio::Device / Stream construction.
# Step-8 additions: the GoalDialog route from a tutorial (scenario) game's PAUSE menu (id 10; the
# owed route since B.3.6 — the run stays up with the dialog open, so it is timed out and judged by
# crash log + the "-> OpenGoal" line), the tool-harness routes (placement / road / removal), and the
# tutorial-load entity census line ([TownCensus] LOAD exit).
set -u
REPO=/c/code/GIT/untitled_rev_eng_project
G="/c/GOG Games/Banished"
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$HERE/runs/stepclose"; mkdir -p "$OUT"
SUM="$OUT/summary.txt"; : > "$SUM"
MK=/c/msys64/ucrt64/bin/mingw32-make.exe
say() { echo "$*" | tee -a "$SUM"; }
cd "$REPO" || exit 2
say "== stepclose @ $(git rev-parse --short HEAD) $(date +%H:%M:%S)"

# 1. Mods route with .pkm mounted (own lock)
bash "$HERE/mods_route.sh" stepclose_mods > "$OUT/mods.log" 2>&1
say "mods route: $(grep -E '^exit=|crashlog bytes|^hang' "$OUT/mods.log" | tr '\n' ' ') $(grep -h 'RtResolve\] ' "$OUT/mods.log" | tail -1)"

# 2. front-end routes + in-game save
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
  E="$OUT/$name/standalone_error.log"; D="$OUT/$name/standalone_debug.log"
  local trap_n; trap_n=$(cat "$E" "$D" 2>/dev/null | grep -c '\[RtMint\] TRAP')
  say "route $name: exit $rc crashlog=$crash hang=$hang killed=$killed (${i}s) TRAP=$trap_n $(cat "$E" "$D" 2>/dev/null | grep '\[RtResolve\]' | grep -i miss | tail -1)"
  say "  automenu: $(cat "$E" "$D" 2>/dev/null | grep -i 'AutoMenu' | tail -3 | tr '\n' '|')"
}
route menuquit     300 BANISHED_AUTOMENU_BUTTON=6
route options      300 BANISHED_AUTOMENU_BUTTON=4 BANISHED_AUTOMENU_CLICKS=m6 BANISHED_MODAL_ANSWER=1
route achievements 300 BANISHED_AUTOMENU_BUTTON=8 BANISHED_AUTOMENU_CLICKS=m6 BANISHED_MODAL_ANSWER=1
# GoalDialog from a scenario game's pause menu: stays open (no quit) -> killed at the timeout, expected
route goaldialog 200 BANISHED_AUTOTUTORIAL=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=240 BANISHED_PAUSEMENU_BUTTON=10
say "  goaldialog: $(cat "$OUT/goaldialog"/standalone_*.log 2>/dev/null | grep -c -- '-> OpenGoal (GoalDialog)') OpenGoal line(s)"
route tool_placement 400 BANISHED_AUTOSTART=1 BANISHED_TOOL_HARNESS=placement BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=120
route tool_road      400 BANISHED_AUTOSTART=1 BANISHED_TOOL_HARNESS=road BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=120
route tool_removal   400 BANISHED_AUTOSTART=1 BANISHED_TOOL_HARNESS=removal BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=120
say "  tools: $(cat "$OUT"/tool_*/standalone_*.log 2>/dev/null | grep -E 'ToolHarness\].*(CREATE|placed|sequence complete)' | tr '\n' '|' | cut -c1-400)"
# in-game save through Save::SaveLoad (file mode: ~600 sim frames, then a save; whole-file DIFF vs
# the input is EXPECTED in this mode — thumbnail and timestamp — what matters is exit 0, no crash)
GATE_MODE=file tools/resave_gate/resave_gate.sh Gate "$OUT/savefile" > "$OUT/savefile.log" 2>&1; rc=$?
say "in-game save (resave file mode): exit $rc $(grep -E 'SAVE OPERATION COMPLETE|crash log bytes|verdict' "$OUT/savefile.log" | tr '\n' ' ')"
tools/game_dir_lock.sh release coord; trap - EXIT

# 3. one functional stress run (record, never chase)
$MK sa-stress > "$OUT/stress.log" 2>&1; rc=$?
say "sa-stress: exit $rc"
grep -iE "frames|fps|ms/frame|median|mean|RESULT|crash|hang|exit" "$OUT/stress.log" | tail -12 | tee -a "$SUM"

# 4. true release compile from a clean obj dir, FOLLOWED by make clean
$MK clean > "$OUT/clean1.log" 2>&1
$MK -j4 standalone > "$OUT/build_release.log" 2>&1; rc=$?
say "true release compile: exit $rc, $(grep -c -E -- ' -c ' "$OUT/build_release.log") files compiled, exe $(stat -c %s standalone/build/reimpl-x64.exe 2>/dev/null) bytes, warnings $(grep -c 'warning:' "$OUT/build_release.log")"
$MK clean > "$OUT/clean2.log" 2>&1

# 5. clean rebuild of debug/perf/unit -> then gate.sh skipbuild does the warning diff, routes, resave
CL="$HERE/runs/closeclean"; mkdir -p "$CL"
for t in standalone-debug standalone-perf unit; do
  $MK -j4 $t > "$CL/build_$t.log" 2>&1; rc=$?; say "clean rebuild $t: exit $rc"
done
say "== gate closeclean (skipbuild) =="
bash "$HERE/gate.sh" closeclean 3 skipbuild > "$OUT/gate_closeclean.log" 2>&1; rc=$?
say "gate closeclean exit $rc"
grep -E "GATE RESULT|new warnings|unit:|route .*exit|resave:|typetable ==|!!" "$HERE/runs/closeclean/summary.txt" | tee -a "$SUM"
# 6. Audio::Device / Stream constructed on the plain new-game route (the stress run is silent by default)
NG="$HERE/runs/closeclean/newgame"
say "audio on new-game: $(cat "$NG/standalone_error.log" "$NG/standalone_debug.log" 2>/dev/null | grep -iE 'Audio::Stream ATTENDANCE|audioDevice=|Stream::Run calls' | head -3 | tr '\n' '|')"
say "NOT IMPLEMENTED stubs on new-game: $(cat "$NG/standalone_error.log" "$NG/standalone_debug.log" 2>/dev/null | grep -c 'NOT IMPLEMENTED')"
# the stub SET against the baseline new-game log (a deleted row still read at boot shows up here)
cat "$NG/standalone_error.log" "$NG/standalone_debug.log" 2>/dev/null | grep 'NOT IMPLEMENTED' | grep -o -E '0x[0-9A-Fa-f]+' | LC_ALL=C sort -u > "$OUT/stubset_now.txt"
cat "$HERE/runs/baseline/newgame/standalone_error.log" "$HERE/runs/baseline/newgame/standalone_debug.log" 2>/dev/null | grep 'NOT IMPLEMENTED' | grep -o -E '0x[0-9A-Fa-f]+' | LC_ALL=C sort -u > "$OUT/stubset_base.txt"
say "stub set: $(LC_ALL=C comm -13 "$OUT/stubset_base.txt" "$OUT/stubset_now.txt" | wc -l) new, $(LC_ALL=C comm -23 "$OUT/stubset_base.txt" "$OUT/stubset_now.txt" | wc -l) gone vs baseline"
LC_ALL=C comm -13 "$OUT/stubset_base.txt" "$OUT/stubset_now.txt" | head -10 | sed 's/^/  NEW /' | tee -a "$SUM"
TU="$HERE/runs/closeclean/tutorial"
say "tutorial census: $(cat "$TU/standalone_error.log" "$TU/standalone_debug.log" 2>/dev/null | grep -h 'TownCensus\] LOAD exit' | head -1)"
git checkout -- standalone/rt_export_addrs.inc
say "== done $(date +%H:%M:%S)"
