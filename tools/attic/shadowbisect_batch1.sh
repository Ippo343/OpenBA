#!/usr/bin/env bash
# Wave 10 lane SHADOWBISECT — batch 1: the DIRECT test of the bypassing-free
# hypothesis, and the gap detector's own negative control.
set -u
cd "$(dirname "$0")/.."
E="BANISHED_RT_DIFF=1 BANISHED_RT_ALLOC_SHADOW=1 BANISHED_RT_ENGINELIFE=0"
A="BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=60"

tools/shadowbisect_cell.sh deploy || exit 9

# b1 — default ported surface, the s1 cell with the gap detector live.
tools/shadowbisect_cell.sh cell b1 900 -- $E $A

# b1ctl — same, plus the gap detector's negative control.
tools/shadowbisect_cell.sh cell b1ctl 900 -- $E $A BANISHED_RT_ALLOC_SHADOW_GAPCTL=1

# b1port0 — the known-green arm, to confirm the detector reads clean there.
tools/shadowbisect_cell.sh cell b1port0 900 -- $E BANISHED_RT_PORT=0 $A
