#!/usr/bin/env bash
# Wave 10 lane SHADOWBISECT — batch 2: the one-binary A/B that attributes the
# red, then SHADOWRE's full seven-cell grid re-taken through the fix.
set -u
cd "$(dirname "$0")/.."
E="BANISHED_RT_DIFF=1 BANISHED_RT_ALLOC_SHADOW=1 BANISHED_RT_ENGINELIFE=0"
A="BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=60"

tools/shadowbisect_cell.sh deploy || exit 9

# ---- the A/B, one binary, one env var ------------------------------------
# f1  : fixed route  -> expected gaps=0 divergences=0
# f1leg: legacy direct call reinstated -> expected gaps>0 divergences>0
tools/shadowbisect_cell.sh cell f1    900 -- $E $A
tools/shadowbisect_cell.sh cell f1leg 900 -- $E $A BANISHED_RT_ALLOC_LEGACY_DIRECTFREE=1

# ---- SHADOWRE section 4, E2's three cells (512MB arena) -------------------
# s1 == f1 above (same invocation); s3 is the save-load cell.
tools/shadowbisect_cell.sh cell s3 900 -- $E BANISHED_AUTOLOAD=Save9 \
    BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=60

# ---- E3's growth cells (16MB arena) --------------------------------------
G="BANISHED_ARENA_MB=16"
tools/shadowbisect_cell.sh cell g1 900 -- $E $G $A
tools/shadowbisect_cell.sh cell g2 900 -- $E $G BANISHED_AUTOLOAD=Save9 \
    BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=60

# ---- the stress cells (SHADOWRE s2 / g3) ---------------------------------
S="BANISHED_STRESS=1 BANISHED_STRESS_FRAMES=1500"
tools/shadowbisect_cell.sh cell s2 1800 -- $E $S
tools/shadowbisect_cell.sh cell g3 1800 -- $E $G $S

# ---- the two negative controls, still required to go red -----------------
tools/shadowbisect_cell.sh cell s4 900 -- $E $A BANISHED_RT_ALLOC_SHADOW_NEGCTL=size
tools/shadowbisect_cell.sh cell g4 900 -- $E $G $A BANISHED_RT_ALLOC_SHADOW_NEGCTL=grow
