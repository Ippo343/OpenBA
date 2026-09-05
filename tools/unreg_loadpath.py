import re,sys
sys.path.insert(0,'tools')
from dispatch_slot_map import slotmap
rows=[]
for l in open(sys.argv[1]):
    m=re.search(r'0x([0-9a-f]+) first=(\w+) total=(\d+)(.*)',l)
    if not m: continue
    a=int(m.group(1),16); ph=dict(re.findall(r'(\w+)=(\d+)',m.group(4)))
    ld=int(ph.get('load',0))+int(ph.get('postload',0))
    if ld: rows.append((ld,a,m.group(2)))
rows.sort(reverse=True)
for ld,a,first in rows:
    e=slotmap.get(a)
    kind = e[1] if e else ("<.data global>" if a>=0x1401c1000 else "<.rdata vtable/const>" if a>=0x140151000 else "<exe code fn>")
    print(f"{ld:7d}  0x{a:09x}  first={first:9s} {kind}")
