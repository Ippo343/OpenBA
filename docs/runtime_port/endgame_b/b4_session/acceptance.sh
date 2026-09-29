#!/usr/bin/env bash
# The registry teardown: acceptance (B3R_RULES §6, INVENTORY_F §6e R1-R17).
# usage: acceptance.sh [stepclose-newgame-log-dir]      run from the repo root; read-only.
# Counts CODE lines (codegrep.py: comments stripped). Exit 0 when every line is at its target.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PY=python
PROD="code runtime standalone engine dx11"
fail=0
n() { $PY "$HERE/codegrep.py" "$1" ${2:-$PROD} --count | sed -n 's/^# \([0-9]*\) code.*/\1/p'; }
line() { # id, description, value, target
  local ok=ok; [ "$3" = "$4" ] || { ok=FAIL; fail=$((fail+1)); }
  printf '%-4s %-4s %-78s %s (target %s)\n' "$1" "$ok" "$2" "$3" "$4"
}
line R1  'RegisterRuntimeExport( + RegisterRuntimeAddr( in PROD'            "$(n 'RegisterRuntime(Export|Addr)\s*\(')" 0
line R2  'RegisterExeAddr( + Register*Slot( in PROD'                         "$(n 'RegisterExeAddrs?\s*\(|Register(Dispatch|Lane3|PlacePlayer)Slot\s*\(')" 0
line R3  'ResolveExport( / RT_EXPORT_ONCE / RtResolveCached / RT_DATA_SLOT in PROD + tests' \
         "$(n 'ResolveExport\s*\(|RT_EXPORT_ONCE|RtResolveCached|RT_DATA_SLOT' "$PROD tests")" 0
line R4  'ResolveAddr(Module::Runtime + RtAddr< + RtPortLookup in PROD + tests' \
         "$(n 'ResolveAddr\s*\(\s*Module::Runtime|RtAddr\s*<|RtPortLookup' "$PROD tests")" 0
line R5  'ResolveAddr( in PROD + tests (any module)'                         "$(n '\bResolveAddr\s*\(' "$PROD tests")" 0
line R5b 'AtomicExport / AtomicAddr / enum class Module in PROD + tests'     "$(n 'AtomicExport|AtomicAddr|enum\s+class\s+Module\b' "$PROD tests")" 0
files=0
for f in standalone/rt_export_addrs.inc standalone/rt_data_mint.cpp standalone/rt_data_mint.h \
         standalone/rt_data_mint_tables.inc standalone/resolve.h standalone/resolve_standalone.cpp \
         standalone/address_registry.cpp standalone/address_registry.h runtime/funcs/rt_resolve_cache.cpp \
         engine/funcs/engine_funcs.h engine/funcs/engine_funcs.cpp tools/rt_mint_gen.py \
         tools/dispatch_slot_map.py tests/unit/rt_port_stub.cpp; do
  [ -e "$f" ] && { files=$((files+1)); echo "     still exists: $f"; }
done
for f in standalone/reg_*.cpp; do [ -e "$f" ] && { files=$((files+1)); echo "     still exists: $f"; }; done
line R6  'registry / mint / resolver / engine_funcs files that exist'        "$files" 0
line R7  'engine/funcs directory entries'                                    "$(ls engine/funcs 2>/dev/null | wc -l | tr -d ' ')" 0
line R8  'g_unitResolveAddr in tests'                                        "$(n 'g_unitResolveAddr' tests)" 0
line R9  'literal 0x140152b60 / 0x140152b68 in PROD'                         "$(n '0x140152[bB]6[08]')" 0
line R10 'literal 0x140089470 in a Register call'                            "$(n 'Register\w*\s*\(\s*0x140089470')" 0
line R11 'kit greps for RtMint TRAP / GATE WARN'                             "$(grep -v -E '^\s*#' "$HERE/gate.sh" | grep -c -E 'RtMint\\?\] TRAP|GATE WARN' | tr -d ' ')" 0
line R12 'lazy resolvers: X_ResolveFuncs definitions/calls in PROD'          "$(n '\w+_ResolveFuncs\s*\(')" 0
line R13 'engine_funcs.h includes in PROD + tests'                           "$(n 'engine_funcs\.h' "$PROD tests")" 0
D="${1:-}"
if [ -n "$D" ] && [ -d "$D" ]; then
  L="$(cat "$D"/standalone_debug.log "$D"/standalone_error.log 2>/dev/null)"
  line R14 'new-game log: [RtUnresolved] / [RtResolve] MISS / [RtMint] / [RtPort] runtime registry / [AddressRegistry]' \
    "$(echo "$L" | grep -c -E '^\[RtUnresolved\]|\[RtResolve\].*MISS|\[RtMint\]|\[RtPort\] runtime registry|\[AddressRegistry\]')" 0
  line R15 'new-game log: NOT IMPLEMENTED lines'                             "$(echo "$L" | grep -c 'NOT IMPLEMENTED')" 0
  line R16 'new-game log: [ExtLoad] calls=5683 openFileFail=0'               "$(echo "$L" | grep -c -E '\[ExtLoad\] calls=5683 openFileFail=0')" 1
else
  echo "R14-R16 skipped: pass the stepclose new-game log directory"
fi
echo "acceptance: $fail line(s) off target"
[ "$fail" = 0 ]
