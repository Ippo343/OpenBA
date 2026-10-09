#!/usr/bin/env bash
# Mods route with .pkm mounted. EDIT WITH THE Write TOOL ONLY.
# usage: mods_route.sh <label>
set -u
REPO=/c/code/GIT/untitled_rev_eng_project
G="/c/GOG Games/Banished"
OUT="$(dirname "$0")/runs/$1"
mkdir -p "$OUT"
cd "$REPO" || exit 2
tools/game_dir_lock.sh acquire coord 1800 || { echo "lock failed"; exit 1; }
restore() {
  mv "$G/WinData/"*.pkm "$G/WinData/_mods_off/" 2>/dev/null
  tools/game_dir_lock.sh release coord
}
trap restore EXIT
cp standalone/build/reimpl-x64.exe "$G/reimpl-x64.exe"
mv "$G/WinData/_mods_off/"*.pkm "$G/WinData/"
ls "$G/WinData/"
rm -f "$G"/standalone_debug.log "$G"/standalone_error.log "$G"/standalone_crash.log "$G"/standalone_hang.log
( cd "$G" && BANISHED_AUTOMENU_BUTTON=7 BANISHED_AUTOMENU_CLICKS=1001,5001,d0,4001,101,m6 BANISHED_MODAL_ANSWER=1 \
    timeout 300 ./reimpl-x64.exe )
echo "exit=$?"
for f in standalone_debug.log standalone_error.log standalone_crash.log standalone_hang.log; do
  [ -f "$G/$f" ] && cp "$G/$f" "$OUT/"
done
echo "crashlog bytes: $(stat -c %s "$G/standalone_crash.log" 2>/dev/null || echo 0)"
echo "hang: $([ -f "$G/standalone_hang.log" ] && echo YES || echo no)"
grep -c "\[RtMint\] TRAP" "$OUT/standalone_debug.log"
grep -h "RtResolve\] " "$OUT/standalone_debug.log" "$OUT/standalone_error.log" | tail -2
grep -hi "ModDialog\|ModDetails\|\[Mod\|pkm\|AutoMenu" "$OUT/standalone_error.log" "$OUT/standalone_debug.log" | head -40
