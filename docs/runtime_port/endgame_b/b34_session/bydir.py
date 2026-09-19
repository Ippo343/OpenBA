"""Per-directory guard-occurrence counts for the guard-bearing global doors,
and per-directory file counts for the file-local door caches."""
import os, re, collections
ns = {}
exec(open('sweep.py').read().split('def main()')[0], ns)
strip = ns['strip']; call_sites = ns['call_sites']; files = ns['files']
SLOT_RE = ns['SLOT_RE']; ROOT = ns['ROOT']
ANY_ADDR = re.compile(r"0x1[0-9a-f]{8,}", re.I)
MANGLED = r"[?@][A-Za-z_0-9]+@(Serializer|FileSystem|Modification|Persistence|PackageFile|ReaderWriter|Encoding)@IO@@"

GLOBALS = ["g_fn_Serializer_Ctor", "g_fn_Serializer_Dtor", "g_fn_Serialize_Int",
           "g_fn_Serialize_Bool", "g_fn_Serializer_GetVersion",
           "fn_Serializer_Serialize_Float", "fn_Serializer_Serialize_Uint",
           "fn_Serializer_GetEncoding", "fn_Serializer_SerializeBulk",
           "fn_InternalBasePtr_Serialize", "SerializerEncodingVCall",
           "fn_Ser_Slot_SerByte_410", "fn_Ser_Slot_SerSizedRaw_3F8",
           "fn_Ser_Slot_SerSizedRaw_400",
           "fn_Persistence_GetSavePath", "fn_Persistence_WriteSettings",
           "fn_Persistence_ReadSettings", "fn_FileSystem_GetDataDirectory",
           "fn_Modification_GetCount", "fn_Modification_GetMod"]

def bucket(rel):
    if rel.startswith("code/Game/Component/"): return "Component"
    if rel.startswith("code/Game/Game/"): return "Game"
    if rel.startswith("code/Game/Action/") or rel.startswith("code/Game/Work/"): return "Action+Work"
    if rel.startswith("code/Game/"): return "rest(code/Game)"
    if rel.startswith("engine/"): return "engine"
    if rel.startswith("standalone/"): return "standalone"
    return "other"

gocc = collections.defaultdict(lambda: collections.defaultdict(int))
gcall = collections.defaultdict(int)
gsites = collections.defaultdict(list)
localfiles = collections.defaultdict(set)
localnames = collections.defaultdict(int)
localdetail = collections.defaultdict(list)

for path in files():
    try: src = open(path, encoding="utf-8", errors="replace").read()
    except OSError: continue
    rel = os.path.relpath(path, ROOT).replace("\\", "/")
    code = strip(src)
    b = bucket(rel)
    for n in GLOBALS:
        c = len(re.findall(r"\b" + re.escape(n) + r"\b", code))
        if c:
            gocc[n][b] += c
            gcall[n] += len(call_sites(code, n))
            gsites[n].append(rel)
    if not (SLOT_RE.search(code) or re.search(MANGLED, src)): continue
    stmts, cur, start, line = [], [], 1, 1
    for ch in code:
        if ch == '\n': line += 1
        if ch in ';{}':
            s = ''.join(cur).strip()
            if s: stmts.append((start, s))
            cur, start = [], line
        else: cur.append(ch)
    mang = {i for i, rl in enumerate(src.split('\n'), 1)
            if re.search(r"(ResolveExport|ResolveFn|ResolveExref)", rl) and re.search(MANGLED, rl)}
    ev = []
    for (ln0, st) in stmts:
        nl = st.count('\n')
        lhs = re.findall(r"\b([A-Za-z_]\w*)\s*=(?![=])", st)
        if not lhs: continue
        hs = SLOT_RE.search(st); hm = any(l in mang for l in range(ln0, ln0 + nl + 2))
        oa = ANY_ADDR.search(st) and not hs
        toks = [t for t in dict.fromkeys(re.findall(r"[A-Za-z_]\w*", st)) if t not in lhs]
        for L in lhs:
            ev.append((ln0, L, 'ser' if hs else ('ser' if hm else ('other' if oa else ('via' if toks else 'other'))),
                       hs.group(0) if hs else ('mangled' if hm else toks)))
    bn = {}
    for e in ev: bn.setdefault(e[1], []).append(e)
    def res(nm, before, d=0, seen=None):
        if d > 5: return None
        seen = (seen or set()) | {nm}
        evs = bn.get(nm)
        if not evs: return None
        pr = [e for e in evs if e[0] <= before]
        e = pr[-1] if pr else evs[0]
        if e[2] == 'ser': return e
        if e[2] == 'other': return None
        for t in e[3]:
            if t in seen: continue
            if res(t, e[0], d + 1, seen): return e
        return None
    for nm, evs in bn.items():
        if not re.match(r"^(s_|g_|fn_|cached)", nm): continue
        if not any(res(nm, e[0] + 1) for e in evs): continue
        localfiles[b].add(rel); localnames[b] += 1
        localdetail[b].append((rel, nm, len(call_sites(code, nm))))

print("### GUARD-BEARING GLOBALS: occurrences per bucket (calls in brackets)")
print(f"{'door':34s} {'CALLs':>5s}  " + "  ".join(f"{k:>14s}" for k in
      ["Component","Game","Action+Work","rest(code/Game)","engine","standalone"]))
for n in GLOBALS:
    row = gocc[n]
    print(f"{n:34s} {gcall[n]:5d}  " + "  ".join(f"{row.get(k,0):14d}" for k in
          ["Component","Game","Action+Work","rest(code/Game)","engine","standalone"]))
print()
print("### FILE-LOCAL DOOR CACHES")
for b in sorted(localfiles):
    print(f"{b:18s} files={len(localfiles[b]):4d}  names={localnames[b]:4d}")
print()
print("### per-bucket file list (local caches)")
for b in sorted(localfiles):
    print(f"-- {b}")
    for f in sorted(localfiles[b]):
        names = [(nm, c) for (r, nm, c) in localdetail[b] if r == f]
        print(f"   {f}   " + " ".join(f"{nm}({c})" for nm, c in names))
