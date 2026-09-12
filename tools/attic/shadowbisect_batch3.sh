#!/usr/bin/env bash
# Wave 10 lane SHADOWBISECT — batch 3: the switch bisect's terminal cell.
# Run with the LEGACY control ON (so the defect is live) and the one wave switch
# that reaches the registration turned off. It is CONFOUNDED by construction --
# BANISHED_RT_SYSTEM=0 removes 87 exports as well as the one registration -- and
# that is the point: this is as far as the kill switches can narrow it.
set -u
cd "$(dirname "$0")/.."
E="BANISHED_RT_DIFF=1 BANISHED_RT_ALLOC_SHADOW=1 BANISHED_RT_ENGINELIFE=0"
A="BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=60"
L="BANISHED_RT_ALLOC_LEGACY_DIRECTFREE=1"

tools/shadowbisect_cell.sh deploy || exit 9
tools/shadowbisect_cell.sh cell bis_sys0 900 -- $E $A $L BANISHED_RT_SYSTEM=0
