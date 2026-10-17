#!/usr/bin/env bash
set -u
REPO=/c/code/GIT/untitled_rev_eng_project; H=$REPO/docs/runtime_port/endgame_b/b8_session
cd "$REPO" || exit 2
rm -f docs/runtime_port/resave_gate/golden/Gate.resaved.bin
tools/game_dir_lock.sh acquire coord 1800 >/dev/null || { echo "lock failed"; exit 1; }
tools/resave_gate/resave_gate.sh Gate "$H/runs/recut1" > "$H/runs/recut1.log" 2>&1; echo "recut1: exit $? $(grep -hE 'GOLDEN CREATED|verdict' "$H/runs/recut1.log" | head -2 | tr '\n' ' ')"
tools/resave_gate/resave_gate.sh Gate "$H/runs/recut2" > "$H/runs/recut2.log" 2>&1; echo "recut2: exit $? $(grep -hE 'verdict|differing' "$H/runs/recut2.log" | head -2 | tr '\n' ' ')"
tools/game_dir_lock.sh release coord >/dev/null
md5sum docs/runtime_port/resave_gate/golden/Gate.resaved.bin
bash "$H/gate.sh" RESFIX 5 resaveonly > "$H/gate_RESFIX.out" 2>&1
grep -E "^resave|GATE RESULT" "$H/runs/RESFIX/summary.txt"
bash "$H/uiroutes.sh" RES_ui > /dev/null 2>&1; cut -c1-200 "$H/runs/RES_ui/summary.txt"
echo RECUT-DONE
