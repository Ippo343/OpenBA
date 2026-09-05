#!/usr/bin/env bash
# tools/ach_reset.sh -- clear the persisted achievement awards.
#
# AchievementManager::SetAchievementMet writes Persistence key "Ach<N>" = true, and
# ReadAchievements loads them back at startup. AchievementManager::Reset does NOT
# clear them (it only rebuilds the instance array), so once an achievement has been
# awarded on this machine every later run inherits it -- and the manager SKIPS
# already-awarded achievements, which makes a re-test look like a pass.
# Run this before any achievement verification run.
KEY='HKCU\Software\Shining Rock Software LLC\Banished'
n=0
for v in $(reg query "$KEY" 2>/dev/null | awk '$1 ~ /^Ach[0-9]+$/ {print $1}'); do
    # MSYS2 bash rewrites a lone /flag into a path; // survives as /.
    reg delete "$KEY" //v "$v" //f >/dev/null 2>&1 && n=$((n+1))
done
echo "cleared $n awarded achievement(s)"
