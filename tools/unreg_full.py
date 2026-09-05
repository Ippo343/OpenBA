"""Full annotated view of a BANISHED_UNREG_CENSUS dump.

unreg_loadpath.py only prints rows with load/postload hits. This prints every
row, grouped by category, with the PE import name where the address is an
import slot. Usage: unreg_full.py <census.txt>
"""
import re, sys
sys.path.insert(0, 'tools')
from dispatch_slot_map import slotmap

rows = []
for l in open(sys.argv[1]):
    m = re.search(r'0x([0-9a-f]+) first=(\w+) total=(\d+)(.*)', l)
    if not m:
        continue
    a = int(m.group(1), 16)
    ph = {k: int(v) for k, v in re.findall(r'(\w+)=(\d+)', m.group(4))}
    ph.pop('total', None)
    rows.append((a, m.group(2), int(m.group(3)), ph))

def kind(a):
    e = slotmap.get(a)
    if e:
        return 'import', e[1]
    if a >= 0x1401c1000:
        return 'data', '<.data global>'
    if a >= 0x140151000:
        return 'rdata', '<.rdata vtable/const>'
    return 'code', '<exe code fn>'

groups = {}
for a, first, total, ph in rows:
    k, name = kind(a)
    groups.setdefault(k, []).append((a, first, total, ph, name))

order = ['code', 'import', 'rdata', 'data']
labels = {'code': 'EXE .text FUNCTIONS (need a Ghidra decompile + reimpl)',
          'import': 'PE IMPORT SLOTS (name is known from the import table)',
          'rdata': '.rdata VTABLE / CONST',
          'data': '.data GLOBALS'}

print(f"{len(rows)} distinct unregistered addresses\n")
for k in order:
    g = groups.get(k)
    if not g:
        continue
    inwin = sum(1 for r in g if r[3].get('load', 0) or r[3].get('postload', 0))
    print(f"== {labels[k]}: {len(g)} ({inwin} hit in the load window)")
    for a, first, total, ph, name in sorted(g):
        phs = ' '.join(f'{p}={c}' for p, c in sorted(ph.items()))
        win = ' <<LOAD' if (ph.get('load', 0) or ph.get('postload', 0)) else ''
        print(f"  0x{a:09x} first={first:<9s} total={total:<6d} {phs:<40s} {name}{win}")
    print()
