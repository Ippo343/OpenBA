#!/bin/bash
# Lane DLG8 run driver. $1 = extra env assignments (space separated), rest is env.
set -e
WT="/c/code/GIT/untitled_rev_eng_project/.claude/worktrees/agent-ae97121fb0fd23555"
echo "built  md5: $(md5sum "$WT/standalone/build/reimpl-x64.exe" | cut -d' ' -f1)"
cp "$WT/standalone/build/reimpl-x64.exe" "/c/GOG Games/Banished/reimpl-x64.exe"
echo "deployed md5: $(md5sum "/c/GOG Games/Banished/reimpl-x64.exe" | cut -d' ' -f1)"
cd "/c/GOG Games/Banished"
rm -f standalone_debug.log standalone_error.log standalone_crash.log
echo "ENV: $DLG8_ENV"
env $DLG8_ENV ./reimpl-x64.exe || echo "exit=$?"
echo "crashlog bytes: $(stat -c %s standalone_crash.log 2>/dev/null || echo 0)"
