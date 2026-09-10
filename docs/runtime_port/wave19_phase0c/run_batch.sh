#!/usr/bin/env bash
set -u
WT="/c/code/GIT/wt-w19-traps"
B="$WT/docs/runtime_port/wave19_phase0c"
"$B/run_shadow.sh" "$B/run2_attend" BANISHED_RT_MINT_SHADOW=1 BANISHED_W17_ATTEND=1 \
    BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120
echo "===== run2 done ====="
"$B/run_shadow.sh" "$B/run3_ctrl" BANISHED_RT_MINT_SHADOW=1 BANISHED_RT_BINWRITER=0 \
    BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120
echo "===== run3 done ====="
