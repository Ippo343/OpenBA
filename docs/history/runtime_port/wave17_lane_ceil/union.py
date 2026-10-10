import re,os,sys
D=os.path.dirname(os.path.abspath(__file__))
files={'save':'attend_save.txt','load':'attend_load.txt','mods':'attend_mods.txt'}
rows={}; seen=set()
for k,f in files.items():
    p=os.path.join(D,f)
    if not os.path.exists(p): sys.exit("MISSING "+p)
    n=0
    for l in open(p):
        m=re.match(r'\[W17Attend\] (ATTENDED|never)\s+init=(\d+)\s+post=(\d+)\s+(\S+)',l)
        if m:
            n+=1; rows.setdefault(m.group(4),{})[k]=(int(m.group(2)),int(m.group(3)))
    if n==0: sys.exit("PARSED 0 ROWS from "+p+" -- refusing to report a union built from nothing")
    seen.add(k); print(f"parsed {n} rows from {f}")
att=sorted(s for s,d in rows.items() if any(v[1]>0 for v in d.values()))
nev=sorted(s for s,d in rows.items() if not any(v[1]>0 for v in d.values()))
print(f"\nroutes: {sorted(seen)}   rows: {len(rows)}")
print(f"UNION ATTENDED: {len(att)}   NEVER on ANY route: {len(nev)}")
print("\n---- NEVER on any route ----")
for s in nev: print("  ",s)
