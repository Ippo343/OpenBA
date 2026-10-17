#!/usr/bin/env bash
# coordinator: build each commit in .claude/worktrees/bisect and run ONE roundtrip resave; print the verdict.
set -u
REPO=/c/code/GIT/untitled_rev_eng_project; WT=$REPO/.claude/worktrees/bisect; MK=/c/msys64/ucrt64/bin/mingw32-make.exe
OUT=$REPO/docs/runtime_port/endgame_b/b8_session/runs/bisect; mkdir -p "$OUT"
for c in "$@"; do
  git -C "$WT" checkout -q "$c" || { echo "$c: checkout failed"; continue; }
  ( cd "$WT" && $MK -j4 standalone-debug > "$OUT/build_$c.log" 2>&1 ) || { echo "$c: BUILD FAILED"; continue; }
  "$REPO/tools/game_dir_lock.sh" acquire coord 1800 >/dev/null || { echo "$c: lock failed"; continue; }
  ( cd "$WT" && tools/resave_gate/resave_gate.sh Gate "$OUT/$c" > "$OUT/resave_$c.log" 2>&1 ); rc=$?
  "$REPO/tools/game_dir_lock.sh" release coord >/dev/null
  echo "$c: resave exit $rc $(grep -h 'differing bytes' "$OUT/resave_$c.log" | head -1) $(git -C "$WT" log -1 --format=%s | cut -c1-70)"
done
echo BISECT-DONE
