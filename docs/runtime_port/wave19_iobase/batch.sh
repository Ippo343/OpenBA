#!/usr/bin/env bash
set -u
WT="/c/code/GIT/wt-w19-iobase"
R="$WT/docs/runtime_port/wave19_iobase"
COMMON="BANISHED_AUTOSTART=1 BANISHED_SAVE_TEST=1 BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=180 BANISHED_RT_MINT_SHADOW=1 BANISHED_W17_ATTEND=1"
echo "########## run2 ARMED (promotions default-on) ##########"
$R/run.sh "$R/run2_armed" $COMMON
echo "########## run3 CONTROL (BANISHED_RT_MINT_PROMOTED=0) ##########"
$R/run.sh "$R/run3_ctrl" $COMMON BANISHED_RT_MINT_PROMOTED=0
echo "########## run4 KILLSWITCH (BANISHED_RT_IOBASE=0) ##########"
$R/run.sh "$R/run4_iobase_off" $COMMON BANISHED_RT_IOBASE=0
