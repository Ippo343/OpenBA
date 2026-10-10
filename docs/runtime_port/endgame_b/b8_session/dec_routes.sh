#!/usr/bin/env bash
# DEC gate extras: tool routes, one stress run, worldcmp vs the oldbase exe (seed 12345).
set -u
cd /c/code/GIT/untitled_rev_eng_project
K=docs/runtime_port/endgame_b/b8_session
bash $K/toolroutes.sh DEC_tools 2>&1 | grep -E '^tool ' | cut -c1-220
/c/msys64/ucrt64/bin/mingw32-make.exe sa-stress > $K/runs/DEC_stress.log 2>&1; echo "sa-stress: exit $?"
grep -iE "frames|fps|ms/frame|median|mean|RESULT|crash|hang|exit" $K/runs/DEC_stress.log | tail -8
bash $K/worldcmp.sh .claude/worktrees/oldbase/standalone/build/reimpl-x64.exe build/debug/reimpl-x64.exe DEC_worldcmp 2>&1 | tail -30
echo "== vs B7 close new.world.txt"; diff docs/runtime_port/endgame_b/b7_session/runs/close_worldcmp/new.world.txt $K/runs/DEC_worldcmp/new.world.txt | head -40
echo "== done"
