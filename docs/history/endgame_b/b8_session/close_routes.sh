#!/usr/bin/env bash
# B8 close extras: worldcmp (oldbase exe vs main's debug exe, then vs the DEC run), one stress run.
set -u
cd /c/code/GIT/untitled_rev_eng_project
K=docs/runtime_port/endgame_b/b8_session
bash $K/worldcmp.sh .claude/worktrees/oldbase/standalone/build/reimpl-x64.exe build/debug/reimpl-x64.exe CLOSE_worldcmp 2>&1 | grep -E '^old|^new|^WORLD|^[<>0-9]' | head -30
echo "== vs DEC new.world.txt"; diff $K/runs/DEC_worldcmp/new.world.txt $K/runs/CLOSE_worldcmp/new.world.txt | head -40; echo "(diff exit $?)"
/c/msys64/ucrt64/bin/mingw32-make.exe sa-stress > $K/runs/CLOSE_stress.log 2>&1; echo "sa-stress: exit $?"
grep -E "fps|p95|crash|hang|RESULT" $K/runs/CLOSE_stress.log | tail -6
echo "== done"
