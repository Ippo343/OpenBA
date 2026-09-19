"""List door pointers with ZERO CALL sites tree-wide.

(A) named global doors  (B) file-local statics/locals bound to a Serializer-family slot
    or a mangled IO export.
Reuses sweep.py's lexer, call_sites() and the positional binding walk.
"""
import os, re
ns = {}
exec(open('sweep.py').read().split('def main()')[0], ns)
strip = ns['strip']; call_sites = ns['call_sites']; files = ns['files']
SLOT_RE = ns['SLOT_RE']; GLOBAL_DOORS = ns['GLOBAL_DOORS']; ROOT = ns['ROOT']
ANY_ADDR = re.compile(r"0x1[0-9a-f]{8,}", re.I)
MANGLED = r"[?@][A-Za-z_0-9]+@(Serializer|FileSystem|Modification|Persistence|PackageFile|ReaderWriter|Encoding)@IO@@"

gcalls = {n: 0 for n in GLOBAL_DOORS}
per_file = {}

for path in files():
    try:
        src = open(path, encoding="utf-8", errors="replace").read()
    except OSError:
        continue
    rel = os.path.relpath(path, ROOT).replace("\\", "/")
    code = strip(src)
    for n in GLOBAL_DOORS:
        if n in code:
            gcalls[n] += len(call_sites(code, n))
    if not (SLOT_RE.search(code) or re.search(MANGLED, src)):
        continue
    stmts = []
    cur, start, line = [], 1, 1
    for ch in code:
        if ch == '\n': line += 1
        if ch in ';{}':
            s = ''.join(cur).strip()
            if s: stmts.append((start, s))
            cur, start = [], line
        else:
            cur.append(ch)
    mangled_lines = {i for i, rl in enumerate(src.split('\n'), 1)
                     if re.search(r"(ResolveExport|ResolveFn|ResolveExref)", rl) and re.search(MANGLED, rl)}
    events = []
    for (ln0, st) in stmts:
        nl = st.count('\n')
        lhs_list = re.findall(r"\b([A-Za-z_]\w*)\s*=(?![=])", st)
        if not lhs_list: continue
        hs = SLOT_RE.search(st)
        hm = any(l in mangled_lines for l in range(ln0, ln0 + nl + 2))
        oa = ANY_ADDR.search(st) and not hs
        toks = [t for t in dict.fromkeys(re.findall(r"[A-Za-z_]\w*", st)) if t not in lhs_list]
        for lhs in lhs_list:
            if hs:      events.append((ln0, lhs, 'ser', hs.group(0)))
            elif hm:    events.append((ln0, lhs, 'ser', 'mangled IO export'))
            elif oa:    events.append((ln0, lhs, 'other', ANY_ADDR.search(st).group(0)))
            elif toks:  events.append((ln0, lhs, 'via', toks))
            else:       events.append((ln0, lhs, 'other', 'unrecognised'))
    by_name = {}
    for e in events: by_name.setdefault(e[1], []).append(e)

    def resolve(name, before, depth=0, seen=None):
        if depth > 5: return None
        seen = seen or set()
        if name in seen: return None
        seen = seen | {name}
        evs = by_name.get(name)
        if not evs: return None
        prior = [e for e in evs if e[0] <= before]
        ev = prior[-1] if prior else evs[0]
        if ev[2] == 'ser': return ev
        if ev[2] == 'other': return None
        for t in ev[3]:
            if t in GLOBAL_DOORS: return ev
            if resolve(t, ev[0], depth + 1, seen): return ev
        return None

    for nm, evs in by_name.items():
        if nm in GLOBAL_DOORS: continue
        # a door pointer = bound to 'ser' at least once and never from 'other'
        if not any(resolve(nm, e[0] + 1) for e in evs): continue
        if not re.match(r"^(s_|g_|fn_|cached)", nm): continue
        n = len(call_sites(code, nm))
        per_file.setdefault(rel, []).append((nm, n, evs[0][0]))

print("=== A. GLOBAL DOORS WITH ZERO CALLS ===")
for n in GLOBAL_DOORS:
    if gcalls[n] == 0:
        print("  ", n)
print("=== A2. global doors still called ===")
for n in GLOBAL_DOORS:
    if gcalls[n]:
        print(f"   {gcalls[n]:4d}  {n}")
print("=== B. FILE-LOCAL DOOR CACHES, per file (name, callcount) ===")
for rel in sorted(per_file):
    rows = per_file[rel]
    zero = [r for r in rows if r[1] == 0]
    live = [r for r in rows if r[1] > 0]
    print(f"{rel}   zero={len(zero)} live={len(live)}")
    for nm, n, ln in zero:
        print(f"      0  {nm}   (first bound :{ln})")
    for nm, n, ln in live:
        print(f"   *{n:3d}  {nm}")
