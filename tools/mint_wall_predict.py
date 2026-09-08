"""Predict which minted vtable slots become TRAPS at cutover.

A RT_MINT_NAMED row carries BOTH a mangled name and a Runtime address:
    { "?OnCompleted@Thread@System@@UEAAXXZ", 0x18000c590ull, RT_MINT_NAMED },

With the DLL present, the name resolves out of the export table. WITH THE DLL
ABSENT there is no export table, so the slot survives only if that ADDRESS (or
that NAME) has a ported body registered in our tree. Otherwise the slot holds
its per-slot trap, and a call through it names itself instead of EXEC-of-0.

So: NAMED rows whose name AND address are both unregistered = the set of slots
that can only ever trap at cutover. That is a PREDICTION of attempt N's walls,
orderable by how likely each is to be called on the boot path.

NOTE ON WHAT THIS CANNOT SEE, stated because every instrument on this project
has had a blind spot it did not announce: registrations whose name or address is
spelled through a VARIABLE or a named constant are invisible here, exactly as
they are to runtime_census.py. So the "unregistered" set is an UPPER BOUND on
the walls, never a lower one. A row it clears is genuinely clear; a row it flags
may still be registered non-literally.
"""
import os
import re
import sys

ROOT = sys.argv[1]
INC = os.path.join(ROOT, "standalone", "rt_data_mint_tables.inc")

named = []
row = re.compile(r'\{\s*"([^"]+)"\s*,\s*(0x[0-9a-fA-F]+)ull\s*,\s*RT_MINT_NAMED\s*\}')
for line in open(INC, encoding="utf-8", errors="replace"):
    m = row.search(line)
    if m:
        named.append((m.group(1), m.group(2).lower()))

print("RT_MINT_NAMED rows: %d" % len(named))

# Harvest every literal registration in the tree.
reg_names = set()
reg_addrs = set()
name_pat = re.compile(r'RegisterRuntimeExport\s*\(\s*"([^"]+)"')
addr_pat = re.compile(r'RegisterRuntimeAddr\s*\(\s*(0x[0-9a-fA-F]+)')
# A ported body may also be reachable because the address appears in a port
# registration table of another shape; count any literal occurrence of the
# address inside runtime/ or standalone/ as evidence and report it separately.
any_addr = {}

for base in ("runtime", "standalone", "engine", "code"):
    d = os.path.join(ROOT, base)
    for dirpath, _, files in os.walk(d):
        for fn in files:
            if not fn.endswith((".cpp", ".h", ".inc")):
                continue
            p = os.path.join(dirpath, fn)
            if p.endswith("rt_data_mint_tables.inc"):
                continue          # the table itself is not a registration
            try:
                t = open(p, encoding="utf-8", errors="replace").read()
            except OSError:
                continue
            for m in name_pat.finditer(t):
                reg_names.add(m.group(1))
            for m in addr_pat.finditer(t):
                reg_addrs.add(m.group(1).lower())
            for m in re.finditer(r"0x1800[0-9a-fA-F]{5}", t):
                any_addr.setdefault(m.group(0).lower(), set()).add(
                    os.path.relpath(p, ROOT))

print("literal RegisterRuntimeExport names: %d" % len(reg_names))
print("literal RegisterRuntimeAddr addrs:   %d" % len(reg_addrs))

hard, soft, ok = [], [], []
for nm, ad in named:
    if nm in reg_names or ad in reg_addrs:
        ok.append((nm, ad))
    elif ad in any_addr:
        soft.append((nm, ad, sorted(any_addr[ad])[:2]))
    else:
        hard.append((nm, ad))

print()
print("=== REGISTERED (name or address) -> survives cutover: %d ===" % len(ok))
print("=== address MENTIONED somewhere but not a literal registration: %d ==="
      % len(soft))
for nm, ad, where in soft[:40]:
    print("  %s  %s" % (ad, nm))
    print("      seen in: %s" % ", ".join(where))
print()
print("=== NO registration and NO mention -> TRAPS AT CUTOVER IF CALLED: %d ==="
      % len(hard))
for nm, ad in hard:
    print("  %s  %s" % (ad, nm))
