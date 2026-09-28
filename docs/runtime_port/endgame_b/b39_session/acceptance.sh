#!/usr/bin/env bash
# B39_RULES.md section 6 -- the acceptance of ENDGAME B.3.9, every line, from the repo root.
#   bash docs/runtime_port/endgame_b/b39_session/acceptance.sh
# Each grep line prints TWO numbers: RAW (the section-6 grep as first written, which counts
# comments -- this tree documents retired names heavily) and CODE (codegrep.py: the same pattern
# over comment-stripped source, string literals kept). The target applies to CODE; RAW is shown
# so nobody mistakes the redefinition for a pass. The type-id line had no working RAW form: its
# `grep -v '^\s*//'` could never match `grep -rn` output, which starts with `file:line:`.
# EDIT WITH THE Write TOOL ONLY.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
CG="python $HERE/codegrep.py"
PY=python; [ -x tools/ghidra_mcp/.venv/Scripts/python.exe ] && PY=tools/ghidra_mcp/.venv/Scripts/python.exe
n() { grep -vc '^# '; }   # matching code lines (drops the scanner's own "# N code line(s)" trailer)
line() { printf '%-4s %-62s RAW %-5s CODE %-5s target %s\n' "$1" "$2" "$3" "$4" "$5"; }

echo "== B39 acceptance @ $(git rev-parse --short HEAD) $(date +%Y-%m-%d\ %H:%M)"
echo "A1   minted vtable rows (kMintVtables):        $(grep -cE 'kVTable[0-9]+, kVTraps[0-9]+' standalone/rt_data_mint_tables.inc)   target 0"
echo "A2   minted ?_typeInfo@ rows:                   $(grep -c '"?_typeInfo@' standalone/rt_data_mint_tables.inc)   target 0"
echo "A3   $($PY tools/rt_mint_gen.py --check 2>&1 | tail -1 | sed -E 's/.*(is [A-Z ]+ \(.*\))/\1/')   target 0 vtables, 0 BaseTypeInfo, 2 storage"
git checkout -- standalone/rt_export_addrs.inc 2>/dev/null
echo "     storage rows: $(grep -oE '\{ "[^"]+"' standalone/rt_data_mint_tables.inc | tr -d '{ "' | tr '\n' ' ')"

line A4 "RT_DELDTOR(0x in runtime/" \
  "$(grep -rn 'RT_DELDTOR(0x' runtime/ | wc -l)" "$($CG 'RT_DELDTOR\(0x' runtime | n)" 0
line A5 "RegisterExeAddr( 0x140153000-0x14016ffff (code standalone engine)" \
  "$(grep -rnE 'RegisterExeAddr\(\s*0x14015[3-9a-f][0-9a-f]{3}|RegisterExeAddr\(\s*0x14016' code standalone engine | wc -l)" \
  "$($CG 'RegisterExeAddr\(\s*0x14015[3-9a-fA-F][0-9a-fA-F]{3}|RegisterExeAddr\(\s*0x14016' code standalone engine --ext=.cpp,.h | n)" 0
line A5b "any Register*( of a literal in that range (all trees)" "-" \
  "$($CG 'Register\w*\(\s*0x14015[3-9a-fA-F][0-9a-fA-F]{3}|Register\w*\(\s*0x14016' code standalone engine runtime dx11 --ext=.cpp,.h,.inc | n)" 0
line A6 "0x1401c.... type-id cell (minus 3dd8/3de0/3de8/22d0/23d0)" "(broken)" \
  "$($CG '0x1401c[0-9a-fA-F]{4}' code engine standalone --ext=.cpp,.h | grep -vE '0x1401c3d(d8|e0|e8)|0x1401c2[23]d0' | n)" 0
echo "     the five non-type-id cells, code lines: $($CG '0x1401c3d(d8|e0|e8)|0x1401c2[23]d0' code engine standalone --ext=.cpp,.h | n)"
line A7 "g_tid_ / g_TypeId_ (code engine standalone)" \
  "$(grep -rn 'g_tid_\|g_TypeId_' code engine standalone --include=*.cpp --include=*.h | wc -l)" \
  "$($CG 'g_tid_|g_TypeId_' code engine standalone --ext=.cpp,.h | n)" 0
echo "A8   factory interpreter files: [$(ls runtime/src/object/rt_factories_* runtime/src/ui/factory_ui1* 2>/dev/null | tr '\n' ' ')]   target nothing"
line A9 '"??_7 literal (code runtime standalone engine dx11)' \
  "$(grep -rn '"??_7' code runtime standalone engine dx11 --include=*.cpp --include=*.h | wc -l)" \
  "$($CG '"\?\?_7' code runtime standalone engine dx11 --ext=.cpp,.h | n)" 0
line A10 "DoorBody (runtime code)" \
  "$(grep -rn 'DoorBody' runtime code | wc -l)" "$($CG 'DoorBody' runtime code | n)" 0
line A11 '"?_singleton@ minus MemoryAllocator' \
  "$(grep -rn '"?_singleton@' code runtime standalone engine dx11 --include=*.cpp --include=*.h | grep -v MemoryAllocator | wc -l)" \
  "$($CG '"\?_singleton@' code runtime standalone engine dx11 --ext=.cpp,.h | grep -v MemoryAllocator | n)" 0
line A12 "InvokeAllRegistrations|REGISTER_ABSTRACT_FACTORY|FactoryRegistration" \
  "$(grep -rn 'InvokeAllRegistrations\|REGISTER_ABSTRACT_FACTORY\|FactoryRegistration' code standalone | wc -l)" \
  "$($CG 'InvokeAllRegistrations|REGISTER_ABSTRACT_FACTORY|FactoryRegistration' code standalone | n)" 0
